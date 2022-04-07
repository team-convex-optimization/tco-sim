extends VehicleBody

# NOTE:
# Libshmemaccess i.e. the interface to shared memory objects only works on 
# linux hence the checks for 'OS.get_name() == "X11"'

# true means that the sim connects to training shmem and writes its state there.
const mode_training = false
# true means that the sim will step "time_step_length" then wait for the shmem 
# state variable to change to 1 which causes the sim to step again.
const stepping = false 
# true means that the sim will use controls from the control shmem to drive. 
# All manual controls will be disabled.
const remote_control = false
const time_step_length = 1.0/30.0 # seconds
const time_reset_settle = 1.0 # seconds
# Used to step and reset
var delta_total = 0.0
var resetting = false
var reset_transform

# Motor constants and methods
const motor_braking_idle = 0.00246 # The deceleration when no power is given to motors TODO: MEASURE ME
const motor_throttle_max = 1.0 # only applies to human input
const max_acceleration = 1.65 # Upper bound for the PID
const max_deceleration = -2.0 # Lower bound for the PID

const MAXRPM = 1000 # Max RPM
var   motor_pid = null # Intantiated on _ready

# Servo motor constants
# TODO: Ackerman steering
const servo_speed = 0.08 #sec/60deg @ 6V (so it takes 0.04 seconds to full steer)
const servo_torque = 9.3 #kgcm @ 6V
const servo_max_voltage = 6
const servo_weight = 0.045 #kg
const servo_frac_per_sec = 8.72664626 # ((60 degrees) / (0.08 seconds)) * ((60 degrees) / (90 degrees)) 

# State vars
var motor_frac = 0
var steer_frac = 0
const steer_angle_max = 40 # degrees
const steer_frac_max = steer_angle_max / 90.0
var shmem_access = null
var shmem_accessible = false

var brake_frac = 0
var motor_brake_max = 0.007

# References to nodes
onready var node_wheel_fl = get_node("CarWheelFL")
onready var node_wheel_fr = get_node("CarWheelFR")
onready var node_wheel_rl = get_node("CarWheelRL")
onready var node_wheel_rr = get_node("CarWheelRR")

func car_reset():
	set_linear_velocity(Vector3(0,0,0))
	set_global_transform(reset_transform)

func input_get():
	steer_frac = 0
	motor_frac = 0.5
	if Input.is_action_pressed("steer_left"):
		steer_frac += -1
	if Input.is_action_pressed("steer_right"):
		steer_frac += 1
		
	if Input.is_action_pressed("accelerate"):
		motor_frac = 1
	if Input.is_action_pressed("decelerate"):
		motor_frac = 0
		
	if Input.is_action_pressed("joy_steer_left"):
		steer_frac += -Input.get_action_strength("joy_steer_left")
	if Input.is_action_pressed("joy_steer_right"):
		steer_frac += Input.get_action_strength("joy_steer_right")
	if abs(steer_frac) < 0.05: #will be ignored in ml
		steer_frac = 0

	if Input.is_action_pressed("joy_accelerate"):
		motor_frac = (Input.get_action_strength("joy_accelerate")/2) + 0.5
	if Input.is_action_pressed("joy_deccelerate"):
		motor_frac -= (Input.get_action_strength("joy_deccelerate")/2)

	steer_frac = -clamp(steer_frac, -steer_frac_max, steer_frac_max)
	motor_frac = clamp(motor_frac, -1, 1)
	motor_frac *= motor_throttle_max

func input_get_shmem():
	var dat = shmem_access.data_read()
	if typeof(dat) == TYPE_REAL_ARRAY and len(dat) > 0:
		shmem_accessible = true
		if typeof(dat[1]) == TYPE_REAL:
			steer_frac = -((dat[1] - 0.5) * 2) * steer_frac_max
		else:
			steer_frac = 0
		if typeof(dat[0]) == TYPE_REAL:
			motor_frac = dat[0]
		else:
			motor_frac = 0
	else:
		shmem_accessible = false
		steer_frac = 0
		motor_frac = 0
		brake_frac = 0

func _ready():
	motor_pid = PID_Controller.new()
	motor_pid.set_pid_values(0.00625, 0.005, 0.0)
	
	set_brake(motor_braking_idle)
	reset_transform = get_global_transform()
	
	if mode_training and OS.get_name() == "X11":
		shmem_access = preload("res://lib_native/libshmemaccess.gdns").new()
		pause_mode = Node.PAUSE_MODE_PROCESS # To avoid pasuing '_process' when game is paused
	else:
		var original_size = OS.window_size
		# Make window big when not training
		OS.set_window_size(Vector2(original_size[0] * 2, original_size[1] * 2))
	
func _process(delta):
	if mode_training and remote_control and stepping and OS.get_name() == "X11":
		var state = shmem_access.state_read()
		if state > 0:
			if state == 1:
				get_tree().paused = false
				delta_total += delta
				if delta_total >= time_step_length:
					delta_total = 0.0;
					shmem_update()
					shmem_access.state_reset()
			elif state == 2:
				if resetting == false:
					car_reset()
					get_tree().paused = false
					resetting = true
				else:
					# Allowing for simulation to settle also avoids condition: 
					# 'p_aabb.position.y > 1e15 || p_aabb.position.y < -1e15'
					delta_total += delta
					if delta_total >= time_reset_settle:
						shmem_update() # Initial observation
						shmem_access.state_reset()
						get_tree().paused = true
						resetting = false
						delta_total = 0.0
			else:
				push_error("Unrecognized state in training shmem")
				get_tree().quit()
		else:
			# Only pause when stepping
			if stepping:
				get_tree().paused = true
			else:
				get_tree().paused = false
	else:
		if mode_training and not stepping:
			shmem_update()
		if Input.is_action_pressed("reset"):
			car_reset()
	
func shmem_update():
	if OS.get_name() == "X11":
		# Update training shmem state
		var wheels_off_track = [
			node_wheel_fl.get_global_transform().origin[1] <= 0.323,
			node_wheel_fr.get_global_transform().origin[1] <= 0.323,
			node_wheel_rl.get_global_transform().origin[1] <= 0.323,
			node_wheel_rr.get_global_transform().origin[1] <= 0.323,
		]
		var drifting = !bool(int(node_wheel_fl.get_skidinfo()) || 
			int(node_wheel_fr.get_skidinfo()) || 
			int(node_wheel_rl.get_skidinfo()) || 
			int(node_wheel_rr.get_skidinfo()))

		# Compute length of velocity vector perpendicular to forward direction of the car
		var speed = -get_transform().basis.xform_inv(get_linear_velocity()).z / 100 # in meters/second

		# Compute the avg RPM of wheels
		var rpm = (node_wheel_fl.get_rpm() + node_wheel_fr.get_rpm() + node_wheel_rl.get_rpm() + node_wheel_rr.get_rpm())/4

		# Global positon of the car
		var pos = get_transform().origin
		
		var video = get_viewport().get_texture().get_data()
		video.flip_y()
		video.convert(Image.FORMAT_L8) # To grayscale
		video = video.get_data() # Convert image to a pool byte array
		
		shmem_access.data_write(wheels_off_track, drifting, speed, pos, video, rpm)

func calculate_motor_output(_motor_frac, delta):
	if _motor_frac == 0.5:
		return 0
	_motor_frac = (_motor_frac - 0.5) * 2 # Normalize from 0 to 1 for good measurement of desired_rpm
	# calc current rpm from avg(rpm)
	var curr_rpm = (node_wheel_fl.get_rpm() + node_wheel_fr.get_rpm() + node_wheel_rl.get_rpm() + node_wheel_rr.get_rpm()) /  4
	# calc desired rpm from RPM_MAX * _motor_frac
	var desired_rpm = _motor_frac * MAXRPM
	# call PID with desired RPM, current RPM and time difference and clamp output to boundaries
	var throttle = clamp(motor_pid.calculate(desired_rpm, curr_rpm, delta), max_deceleration, max_acceleration)
	return throttle

func _physics_process(delta):
	var _brake_frac = 0
	if not resetting:
		var steer_frac_old = steer_frac
		if mode_training and remote_control and (OS.get_name() == "X11"):
			input_get_shmem()
		else:
			input_get()
		if motor_frac > 0.5:
			_brake_frac = 0
		else:
			_brake_frac = (abs(motor_frac - 0.5) * 2 * (motor_brake_max - motor_braking_idle)) + motor_braking_idle

		# Enforce servo speed limit
		var servo_frac_delta = servo_frac_per_sec * delta
		if steer_frac > steer_frac_old:
			steer_frac = clamp(steer_frac_old + servo_frac_delta, -steer_frac_max, steer_frac_max)
		else:
			steer_frac = clamp(steer_frac_old - servo_frac_delta, -steer_frac_max, steer_frac_max)

		set_engine_force(calculate_motor_output(motor_frac, delta))
		set_steering(steer_frac)
		set_brake(_brake_frac)
	else:
		set_steering(0.0)
		set_engine_force(0.0)

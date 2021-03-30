extends VehicleBody

# NOTE:
# Libshmemaccess i.e. the interface to shared memory objects only works on 
# linux hence the checks for 'OS.get_name() == "X11"'

# true means that the sim connects to training shmem and writes its state there.
const mode_training = true
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
const motor_kv =  2270 #rpm/V
const motor_v_max = 7.4
const motor_amp_max = 28
const motor_weight = 0.178 #kg
const motor_frequency = 8 #hz
const motor_inductance =  0.0046 #henry
const motor_resistance = 2 * 3.14 * motor_frequency * motor_inductance
const motor_breaking_idle = 0.00246 # The deceleration when no power is given to motors TODO: MEASURE ME
const motor_throttle_max = 0.4 # only applies to human input

func get_motor_current(voltage, resistance):
	return voltage / resistance

# Current in amps, Velocity_constant in KV
func get_motor_torque(current, velocity_constant):
	return (8.3 * current) / velocity_constant

# Servo motor constants
# TODO: Ackerman steering
const servo_speed = 0.08 #sec/60deg @ 6V (so it takes 0.04 seconds to full steer)
const servo_torque = 9.3 #kgcm @ 6V
const servo_max_voltage = 6
const servo_weight = 0.045 #kg
const servo_frac_per_sec = 8.72664626 # ((60 degrees) / (0.08 seconds)) * ((60 degrees) / (90 degrees)) 

# State vars
var motor_v = 0
var motor_frac = 0
var steer_frac = 0
const steer_frac_max = 0.32 # around 30 degrees
var shmem_access = null
var shmem_accessible = false

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
	motor_frac = 0
	if Input.is_action_pressed("steer_left"):
		steer_frac += -1
	if Input.is_action_pressed("steer_right"):
		steer_frac += 1
		
	if Input.is_action_pressed("accelerate"):
		motor_frac = 1
	if Input.is_action_pressed("decelerate"):
		motor_frac -= 1
		
	if Input.is_action_pressed("joy_steer_left"):
		steer_frac += -Input.get_action_strength("joy_steer_left")
	if Input.is_action_pressed("joy_steer_right"):
		steer_frac += Input.get_action_strength("joy_steer_right")
	if abs(steer_frac) < 0.05: #will be ignored in ml
		steer_frac = 0
		
	if Input.is_action_pressed("joy_accelerate"):
		motor_frac = Input.get_action_strength("joy_accelerate")
	if Input.is_action_pressed("joy_deccelerate"):
		motor_frac -= Input.get_action_strength("joy_deccelerate")
		
	steer_frac = -clamp(steer_frac, -steer_frac_max, steer_frac_max)
	motor_frac = clamp(motor_frac, -1, 1)
	motor_frac *= motor_throttle_max
	if abs(motor_frac) < 0.01:
		motor_v = 0
	else:
		motor_v = motor_frac * motor_v_max

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
	
	if motor_frac == 0:
		motor_v = 0
	else:
		motor_v = ((motor_frac - 0.5) * 2.0) * motor_v_max

func _ready():
	set_brake(motor_breaking_idle)
	reset_transform = get_global_transform()
	
	if mode_training and OS.get_name() == "X11":
		shmem_access = preload("res://lib_native/libshmemaccess.gdns").new()
		pause_mode = Node.PAUSE_MODE_PROCESS # To avoid pasuing '_process' when game is paused
	else:
		var original_size = OS.window_size
		# Make window big when not training
		OS.set_window_size(Vector2(original_size[0] * 2, original_size[1] * 2))
	
func _process(delta):
	if mode_training and remote_control and OS.get_name() == "X11":
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
		
		# Global positon of the car
		var pos = get_transform().origin
		
		var video = get_viewport().get_texture().get_data()
		video.flip_y()
		video.convert(Image.FORMAT_L8) # To grayscale
		video = video.get_data() # Convert image to a pool byte array
		
		shmem_access.data_write(wheels_off_track, drifting, speed, pos, video)

func _physics_process(delta):
	if not resetting:
		var steer_frac_old = steer_frac
		if mode_training and remote_control and (OS.get_name() == "X11"):
			input_get_shmem()
		else:
			input_get()
		
		var amp = get_motor_current(motor_v, motor_resistance)
		var torque = get_motor_torque(amp, motor_kv)
		
		# Enforce servo speed limit
		var servo_frac_delta = servo_frac_per_sec * delta
		if steer_frac > steer_frac_old:
			steer_frac = clamp(steer_frac_old + servo_frac_delta, -steer_frac_max, steer_frac_max)
		else:
			steer_frac = clamp(steer_frac_old - servo_frac_delta, -steer_frac_max, steer_frac_max)
			
		set_steering(steer_frac)
		set_engine_force((torque/0.0325)/1.8) # Force is (torque/moment)
	else:
		set_steering(0.0)
		set_engine_force(0.0)

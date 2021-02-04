extends VehicleBody

# NOTE:
# Libshmemaccess i.e. the interface to shared memory objects only works on 
# linux hence the checks for 'OS.get_name() == "X11"'

const mode_training = false

# Motor constants and methods
const motor_kv =  2270 #rpm/V
const motor_v_max = 7.4
const motor_amp_max = 28
const motor_weight = 0.178 #kg
const motor_frequency = 8 #hz
const motor_inductance =  0.0046 #henry
const motor_resistance = 2 * 3.14 * motor_frequency * motor_inductance

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

# State vars
var motor_v = 0
var motor_frac = 0
var steer_frac = 0
const steer_frac_max = 0.45
var shmem_access = null
var shmem_accessible = false

# References to nodes
onready var node_wheel_fl = get_node("CarWheelFL")
onready var node_wheel_fr = get_node("CarWheelFR")
onready var node_wheel_rl = get_node("CarWheelRL")
onready var node_wheel_rr = get_node("CarWheelRR")

func input_get():
	steer_frac = 0
	motor_frac = 0
	if Input.is_action_pressed("steer_left"):
		steer_frac += -1
	if Input.is_action_pressed("steer_right"):
		steer_frac += 1
		
	if Input.is_action_pressed("accelerate"):
		motor_frac = motor_v_max
	if Input.is_action_pressed("decelerate"):
		motor_frac -= motor_v_max
		
	if Input.is_action_pressed("joy_steer_left"):
		steer_frac += -Input.get_action_strength("joy_steer_left")
	if Input.is_action_pressed("joy_steer_right"):
		steer_frac += Input.get_action_strength("joy_steer_right")
	if abs(steer_frac) < 0.05: #will be ignored in ml
		steer_frac = 0
		
	if Input.is_action_pressed("joy_accelerate"):
		motor_frac = Input.get_action_strength("joy_accelerate") * motor_v_max
	if Input.is_action_pressed("joy_deccelerate"):
		motor_frac -= Input.get_action_strength("joy_deccelerate") * motor_v_max
		
	steer_frac = -clamp(steer_frac, -steer_frac_max, steer_frac_max)
	motor_frac = clamp(motor_frac, -1, 1)
	if motor_frac == 0:
		motor_v = 0
	else:
		motor_v = (1/motor_frac) * motor_v_max

func input_get_shmem():
	var dat = shmem_access.data_read()
	if typeof(dat) != TYPE_NIL and len(dat) > 0:
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
		motor_v = ((motor_frac - 0.5) * 2) * motor_v_max

func _ready():
	if mode_training and OS.get_name() == "X11":
		shmem_access = preload("res://lib_native/libshmemaccess.gdns").new()
	set_brake(0.003) # The deceleration when no power is given to motors TODO: MEASURE ME
	
var delta_total = 0.0
func _process(delta):
	if mode_training and OS.get_name() == "X11":
		var state = shmem_access.state_read()
		if state > 0:
			if state == 1:
				delta_total += delta
				if delta_total >= 1.0:
					shmem_update()
					shmem_access.state_reset()
			elif state == 2:
				get_tree().reload_current_scene()
				shmem_access.state_reset()
			else:
				push_error("Unrecognized state in training shmem")
				get_tree().quit()
		else:
			get_tree().paused = true
	
func shmem_update():
	if OS.get_name() == "X11":
		# Update training shmem state
		var wheels_off_track = [
			node_wheel_fl.get_global_transform().origin[1] <= 0.323,
			node_wheel_fr.get_global_transform().origin[1] <= 0.323,
			node_wheel_rl.get_global_transform().origin[1] <= 0.323,
			node_wheel_rr.get_global_transform().origin[1] <= 0.323,
		]
		var drifting = !bool(int(node_wheel_fl.get_skidinfo()) || int(node_wheel_fr.get_skidinfo()) || int(node_wheel_rl.get_skidinfo()) || int(node_wheel_rr.get_skidinfo()))
		
		# Compute length of velocity vector perpendicular to forward direction of the car
		var speed = -get_transform().basis.xform_inv(get_linear_velocity()).z / 100 # in meters/second
		
		# Global positon of the car
		var pos = get_transform().origin
		
		var video = get_viewport().get_texture().get_data()
		video.convert(Image.FORMAT_L8) # To grayscale
		video = video.get_data() # Convert image to a pool byte array
		
		shmem_access.data_write(wheels_off_track, drifting, speed, pos, video)

func _physics_process(delta):
	if mode_training and (OS.get_name() == "X11"):
		input_get_shmem()
	else:
		input_get()
	
	var amp = get_motor_current(motor_v, motor_resistance)
	var torque = get_motor_torque(amp, motor_kv)
	
	set_steering(steer_frac)
	set_engine_force((torque/0.0325)/1.8) # Force is (torque/moment)

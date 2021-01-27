extends VehicleBody

const mode_autonomous = true

#================ DRIVE MOTOR CONSTANTS AND METHODS =====================#
const motor_kv =  2270 #rpm/V
const motor_v_max = 7.4
const motor_amp_max = 28
const motor_weight = 0.178 #kg
const motor_frequency = 8 #hz
const motor_inductance =  0.0046 #henry
const motor_resistance = 2 * 3.14 * motor_frequency * motor_inductance

func get_motor_current(voltage, resistance):
	return voltage / resistance

#Current in amps, Velocity_constant in KV
func get_motor_torque(current, velocity_constant):
	return (8.3 * current) / velocity_constant

#================ SERVO MOTOR CONSTANTS AND METHODS =====================#
#TODO Ackerman steering for longer base
const servo_speed = 0.08 #sec/60deg @ 6V (so it takes 0.04 seconds to full steer)
const servo_torque = 9.3 #kgcm @ 6V
const servo_max_voltage = 6
const servo_weight = 0.045 #kg

#================ STATE VARS =====================#
var motor_v = 0
var servo_angle = 0
const servo_angle_max = 0.4
var shmem_access = null
var shmem_accessible = false

#================ RUNTIME METHODS ================#

func input_get():
	servo_angle = 0
	motor_v = 0
	if Input.is_action_pressed("steer_left"):
		servo_angle += -1
	if Input.is_action_pressed("steer_right"):
		servo_angle += 1
		
	if Input.is_action_pressed("accelerate"):
		motor_v = motor_v_max
	if Input.is_action_pressed("decelerate"):
		motor_v -= motor_v_max
		
	if Input.is_action_pressed("joy_steer_left"):
		servo_angle += -Input.get_action_strength("joy_steer_left")
	if Input.is_action_pressed("joy_steer_right"):
		servo_angle += Input.get_action_strength("joy_steer_right")
	if abs(servo_angle) < 0.05: #will be ignored in ml
		servo_angle = 0
		
	if Input.is_action_pressed("joy_accelerate"):
		motor_v = Input.get_action_strength("joy_accelerate") * motor_v_max
	if Input.is_action_pressed("joy_deccelerate"):
		motor_v -= Input.get_action_strength("joy_deccelerate") * motor_v_max
		
	servo_angle = -clamp(servo_angle, -servo_angle_max, servo_angle_max)
	motor_v = clamp(motor_v, -motor_v_max, motor_v_max)

func input_get_shmem():
	var dat = shmem_access.get_data()
	if typeof(dat) != TYPE_BOOL and dat.size() > 0:
		shmem_accessible = true
		if typeof(dat[1]) == TYPE_REAL:
			servo_angle = -((dat[1] - 0.5) * 2) * servo_angle_max
		else:
			servo_angle = 0
		if typeof(dat[0]) == TYPE_REAL:
			motor_v = ((dat[0] - 0.5) * 2) * motor_v_max
		else:
			motor_v = 0
	else:
		shmem_accessible = false
		servo_angle = 0
		motor_v = 0

func _ready():
	if OS.get_name() == "X11":
		shmem_access = preload("res://lib_native/libshmemaccess.gdns").new()
	set_brake(0.003) #the decceleration when no power given to motors TODO : MEASURE ME

func _physics_process(delta):
	if mode_autonomous and (OS.get_name() == "X11"):
		input_get_shmem()
	else:
		input_get()
	
	var amp = get_motor_current(motor_v, motor_resistance)
	var torque = get_motor_torque(amp, motor_kv)
	
	set_steering(servo_angle)
	set_engine_force((torque/0.0325)/1.8) #force is (torque/moment) 
	

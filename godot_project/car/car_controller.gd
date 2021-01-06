extends VehicleBody

#================ DRIVE MOTOR CONSTANTS AND METHODS =====================#
const motor_KV =  2270 #rpm/V
const motor_max_V = 7.4
const motor_max_Amperage = 28
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
var motor_V = 0
var servo_angle = 0

#================ RUNTIME METHODS ================#

func input_get():
	servo_angle = 0
	motor_V = 0
	if Input.is_action_pressed("steer_left"):
		servo_angle += -1
	if Input.is_action_pressed("steer_right"):
		servo_angle += 1
		
	if Input.is_action_pressed("accelerate"):
		motor_V = motor_max_V
	if Input.is_action_pressed("decelerate"):
		motor_V -= motor_max_V
		
	if Input.is_action_pressed("joy_steer_left"):
		servo_angle += -Input.get_action_strength("joy_steer_left")
	if Input.is_action_pressed("joy_steer_right"):
		servo_angle += Input.get_action_strength("joy_steer_right")
	if abs(servo_angle) < 0.05: #will be ignored in ml
		servo_angle = 0
		
	if Input.is_action_pressed("joy_accelerate"):
		motor_V = Input.get_action_strength("joy_accelerate") * motor_max_V
	if Input.is_action_pressed("joy_deccelerate"):
		motor_V -= Input.get_action_strength("joy_deccelerate") * motor_max_V
		
	servo_angle = -clamp(servo_angle, -0.5, 0.5)
	motor_V = clamp(motor_V, -motor_max_V, motor_max_V)


func _ready():
	set_brake(0.003) #the decceleration when no power given to motors TODO : MEASURE ME

func _physics_process(delta):
	input_get()
	
	var amp = get_motor_current(motor_V, motor_resistance)
	var torque = get_motor_torque(amp, motor_KV)
	#var rpm = motor_V * motor_KV
	#print_debug("V\t", motor_V)
	#print_debug("A\t", amp)
	#print_debug("T\t", torque)
	#print_debug("RPM\t", rpm)
	#print_debug("FLRPM\t", get_node("CarWheelFL").get_rpm()*1.8)
	#print_debug("servo angle\t",servo_angle)
	
	set_steering(servo_angle)
	set_engine_force((torque/0.0325)/1.8) #force is (torque/moment) 
	

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
var servo_V = 0

#================ RUNTIME METHODS ================#

func input_get():
	if Input.is_action_pressed("steer_left"):
		servo_V = -servo_max_voltage
	elif Input.is_action_pressed("steer_right"):
		servo_V = servo_max_voltage
	else:
		servo_V = 0
		
	if Input.is_action_pressed("motor_accelerate"):
		motor_V = motor_max_V
		if motor_V < motor_max_V:
			motor_V += 0.2
	elif Input.is_action_pressed("motor_decelerate"):
		if motor_V > -motor_max_V:
			motor_V -= 0.2
	else:
		motor_V = 0 #OFF


func _ready():
	set_brake(0.003) #the decceleration when no power given to motors TODO : MEASURE ME

func _physics_process(delta):
	input_get()
	
	var amp = get_motor_current(motor_V, motor_resistance)
	var torque = get_motor_torque(amp, motor_KV)
	var rpm = motor_V * motor_KV
	print_debug("V\t", motor_V)
	print_debug("A\t", amp)
	print_debug("T\t", torque)
	print_debug("RPM\t", rpm)
	
	if servo_V > 0:
		set_steering(-0.5)
	elif servo_V < 0:
		set_steering(0.5)
	else:
		set_steering(0)
	
	if motor_V > 0:
		set_engine_force((torque/0.07)) #force is (torque/moment) * gear ratio
	elif motor_V < 0:
		set_engine_force((torque/0.07))
	else:
		set_engine_force(0)

extends VehicleBody

const motor_torque = 100

func input_get():
	var steer = 0
	var motor = 0
	if Input.is_action_pressed("steer_left"):
		steer -= 1
	if Input.is_action_pressed("steer_right"):
		steer += 1
		
	if Input.is_action_pressed("motor_accelerate"):
		motor += 1
	if Input.is_action_pressed("motor_decelerate"):
		motor -= 1
	return [steer, motor]

func _ready():
	pass

func _physics_process(delta):
	var input = input_get()
	var steer = input[0]
	var motor = input[1]
	
	if steer > 0:
		set_steering(-0.5)
	elif steer < 0:
		set_steering(0.5)
	else:
		set_steering(0)
	
	if motor > 0:
		set_engine_force(delta * motor_torque)
	elif motor < 0:
		set_engine_force(delta * -motor_torque)
	else:
		set_engine_force(0)

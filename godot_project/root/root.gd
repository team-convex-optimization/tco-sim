extends Spatial


# Declare member variables here. Examples:
var is_verbose_on = false
const update_interval = 0.2

#car state that will be updated every update_interval
var total_time = 0
var last_update = -1
var wheel_rpm = [0,0,0,0]
var speed = 0.0

# Called when the node enters the scene tree for the first time.
func _ready():
	pass

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	if (is_verbose_on):
		total_time += delta
		var vehicle = get_node("Car/VehicleBody")
		var motor_voltage = vehicle.motor_v
		var steer_angle = vehicle.servo_angle
		var motor_rpm = vehicle.motor_kv * motor_voltage
		var is_skidding = !bool(int(vehicle.get_node("CarWheelFL").get_skidinfo()) || int(vehicle.get_node("CarWheelFR").get_skidinfo()) || int(vehicle.get_node("CarWheelRL").get_skidinfo()) || int(vehicle.get_node("CarWheelRR").get_skidinfo()))
		
		var text = "motor_voltage : " + str(motor_voltage) + "\nservo_angle : " + str(steer_angle) + "\nmotor_rpm : " + str(motor_rpm) 
		if (last_update + update_interval < total_time):
			last_update = total_time
			wheel_rpm = [int(vehicle.get_node("CarWheelFL").get_rpm()), int(vehicle.get_node("CarWheelFR").get_rpm()), int(vehicle.get_node("CarWheelRL").get_rpm()), int(vehicle.get_node("CarWheelRR").get_rpm())]
			speed = ((2 * 3.14159 * 0.0325 * wheel_rpm[0])*60)/1000
		
		var isFRW_ontrack = get_node("Car/VehicleBody/CarWheelFR").get_global_transform().origin[1] > 0.3199
		var isFLW_ontrack = get_node("Car/VehicleBody/CarWheelFL").get_global_transform().origin[1] > 0.3199
		var isRLW_ontrack = get_node("Car/VehicleBody/CarWheelRL").get_global_transform().origin[1] > 0.31
		var isRRW_ontrack = get_node("Car/VehicleBody/CarWheelRR").get_global_transform().origin[1] > 0.31
		
		text += "\nWheel_rpm (FL,FR,RL,RR): " + str(wheel_rpm[0]) + " " + str(wheel_rpm[1]) + " " + str(wheel_rpm[2]) + " " + str(wheel_rpm[3]) 
		text += "\nSpeed (km/h) : " + str(speed)
		text+= "\nDrifting? : " + str(is_skidding)
		text+= "\nWheel_contact (FL,FR,RL,RR): " + str(isFLW_ontrack) + " " + str(isFRW_ontrack) + " " + str(isRLW_ontrack) + " " + str(isRRW_ontrack) 
		get_node("DebugInfo").text = text


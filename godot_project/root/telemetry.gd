extends RichTextLabel

# Declare member variables here. Examples:
var enabled = true
const update_interval = 0.1

# Car state that will be updated every update_interval
var total_time = 0
var last_update = -1
var wheel_rpm = [0,0,0,0]
var speed = 0.0
var shmem_status = false

# Nodes to get data from
var node_vehicle = null
var node_wheel_fl = null
var node_wheel_fr = null
var node_wheel_rl = null
var node_wheel_rr = null
const telementry_text_format = "motor_voltage: %f\nservo_angle: %f\nmotor_rpm: %f\nwheel_rpm (FL,FR,RL,RR): %f %f %f %f\nspeed (km/h): %f\nskidding: %s\nwheel_contact (FL,FR,RL,RR): %s %s %s %s\nshmem_status: %s"

func _ready():
	node_vehicle = get_node("../Car/VehicleBody")
	node_wheel_fl = node_vehicle.get_node("CarWheelFL")
	node_wheel_fr = node_vehicle.get_node("CarWheelFR")
	node_wheel_rl = node_vehicle.get_node("CarWheelRL")
	node_wheel_rr =node_vehicle.get_node("CarWheelRR")

func _process(delta):
	if enabled:
		total_time += delta
		var motor_voltage = node_vehicle.motor_v
		var steer_angle = node_vehicle.servo_angle
		var motor_rpm = node_vehicle.motor_kv * motor_voltage
		var is_skidding = !bool(int(node_wheel_fl.get_skidinfo()) || int(node_wheel_fr.get_skidinfo()) || int(node_wheel_rl.get_skidinfo()) || int(node_wheel_rr.get_skidinfo()))
		
		if (last_update + update_interval < total_time):
			last_update = total_time
			wheel_rpm = [
				int(node_wheel_fl.get_rpm()), 
				int(node_wheel_fr.get_rpm()), 
				int(node_wheel_rl.get_rpm()), 
				int(node_wheel_rr.get_rpm())
			]
			speed = ((2 * 3.14159 * 0.0325 * wheel_rpm[0])*60)/1000
		
		var fl_ontrack = node_wheel_fl.get_global_transform().origin[1] > 0.3199
		var fr_ontrack = node_wheel_fr.get_global_transform().origin[1] > 0.3199
		var rl_ontrack = node_wheel_rl.get_global_transform().origin[1] > 0.31
		var rr_ontrack = node_wheel_rr.get_global_transform().origin[1] > 0.31
			
		self.text = telementry_text_format % [
			motor_voltage, 
			steer_angle, 
			motor_rpm, 
			wheel_rpm[0], 
			wheel_rpm[1], 
			wheel_rpm[2], 
			wheel_rpm[3], 
			speed, 
			str(is_skidding), 
			str(fl_ontrack), 
			str(fr_ontrack), 
			str(rl_ontrack), 
			str(rr_ontrack),
			str(node_vehicle.shmem_accessible)
		]


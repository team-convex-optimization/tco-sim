extends Spatial

# s = straight, l = left, r = right, e = straight extension, i = intersection
const track_desc = "sllllssrrrrsssllssllsssllsrrllssellsse"

# Offset in format [x,z,deg]
const offsets = {'s':[0,-7.2,0], 'l':[-1.3034237,-3.1465890,45], 'r':[1.3034237,-3.1465890,-45], 'i':[0,-5.5,0], 'e':[0,-1.7,0], 'c':[0,14.4,0]}
const pieces = {'s':preload("res://tracks/pieces/straight.tscn"), 'l':preload("res://tracks/pieces/curve_left.tscn"), 'r':preload("res://tracks/pieces/curve_right.tscn"), 'i':preload("res://tracks/pieces/intersection_center.tscn"), 'e':preload("res://tracks/pieces/intersection_extension.tscn"), 'c':preload("res://tracks/pieces/chicane.tscn")}

func _ready():
	var last_pos = Vector3(0,0,0)
	var last_deg = 0
	for p in track_desc:
		var pNode = pieces[p].instance()
		print(last_pos)
		print(last_deg)
		pNode.transform.origin = Vector3(last_pos.x, 0, last_pos.z)
		pNode.set_rotation_degrees(Vector3(0, last_deg, 0))
		self.add_child(pNode)
		last_pos = pNode.to_global(Vector3(offsets[p][0], 0, offsets[p][1]))
		last_deg += offsets[p][2]
		last_deg %= 360

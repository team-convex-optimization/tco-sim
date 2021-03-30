extends Sprite

# Declare member variables here. Examples:
const noiseUpdate = 0.5 #seconds
var time_elapsed = 0
onready var randOffset = randi() #Ensure we get different images 
onready var noise = OpenSimplexNoise.new()
var txt = ImageTexture.new()

# Called when the node enters the scene tree for the first time.
func _ready():
	# Configure noise params
	noise.seed = randi()
	noise.octaves = 4
	noise.period = 20.0
	noise.persistence = 0.8
	var img = noise.get_image(640, 480)
	txt.create_from_image(img)
	set_texture(txt)

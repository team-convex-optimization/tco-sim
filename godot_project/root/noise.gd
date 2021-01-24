extends Sprite


# Declare member variables here. Examples:
const noiseUpdate = 0.5 #seconds
var time_elapsed = 0
onready var randOffset = randi() #Ensure we get different images 
onready var noise = OpenSimplexNoise.new()

# Called when the node enters the scene tree for the first time.
func _ready():
	# Configure noise params
	noise.seed = randi()
	noise.octaves = 1
	noise.period = 5
	noise.persistence = 0.01

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	time_elapsed += delta
	if (time_elapsed > noiseUpdate):
		noise.seed = randi() + randOffset
		var txt = ImageTexture.new()
		var img = noise.get_image(550, 550)
		txt.create_from_image(img)
		set_texture(txt)
		time_elapsed = 0

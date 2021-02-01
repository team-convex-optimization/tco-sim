extends Button

func _ready():
	self.connect("pressed", self, "handle_press")

func handle_press():
	get_tree().reload_current_scene()

tool
extends EditorPlugin


func _enter_tree():
	add_custom_type("PIDController", "Node", preload("res://addons/PIDController/PIDLogic.gd"), null)


func _exit_tree():
	remove_custom_type("PIDController")

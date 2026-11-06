// Sample of a model with hitbox collision (docs/ENV_MODEL.md "Collision model"): three ammo crates
// (the ammobox mesh at twice its size), two on the floor and one on top of the -X one. $flags 512 makes the
// engine collide with the two $hbox boxes below instead of the entity's box: a player can stand on the low
// step next to the top crate. Use it in env_model with "Solid (model hitboxes)".
$modelname "crate_steps.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "crate_steps"
}

// 512: collide with the hitboxes
$flags 512

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

// the two floor crates (one box), then the top crate; coordinates in the bone's space (root: the model's)
$hbox 0 "root" -78.76 -21.65 0 78.76 21.65 42.36
$hbox 0 "root" -78.76 -21.65 42.36 0 21.65 84.72

$sequence "idle" {
	"..\anims\idle"
	fps 1
}

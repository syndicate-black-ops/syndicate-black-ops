$modelname "v_mini.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "gatleen_ref"
}

$flags 0

$attachment 0 "Bip01 R Hand" 60 -20 21

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -2.28 -1.54 -1.36 11.86 1.15 1.68
$hbox 0 "Bip01 R Arm2" -2.34 -1.6 -2 11.89 1.15 1.13
$hbox 0 "Bone01" -1.99 -15.79 -8.78 19.18 21.91 8.72
$hbox 0 "Bone02" -0.83 -5.11 -4.88 41.790001 5.04 5.25

$sequence "idle" {
	"..\anims\idle"
	ACT_IDLE 5
	fps 10
	loop
}
$sequence "idle2" {
	"..\anims\idle2"
	ACT_IDLE 4
	fps 10
	loop
}
$sequence "spinup" {
	"..\anims\spinup"
	fps 32
}
$sequence "spindown" {
	"..\anims\spindown"
	fps 30
}
$sequence "fire" {
	"..\anims\fire"
	{ event 5001 0 "51" }
	fps 75
	loop
}
$sequence "draw" {
	"..\anims\draw"
	fps 32
}
$sequence "holster" {
	"..\anims\holster"
	fps 32
}

$modelname "v_wong.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "wongref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -13.49 -12.12 -1.8 11.88 0.78 5.09
$hbox 0 "Bip01 R Arm2" -13.85 -11.57 -4.66 11.9 0.86 1.15
$hbox 0 "Bip01 R Hand" -0.35 -0.41 -1.64 4.26 0.62 1.74

$sequence "idle1" {
	"..\anims\idle1"
	fps 30
	loop
}
$sequence "idle2" {
	"..\anims\idle2"
	fps 30
	loop
}
$sequence "idle3" {
	"..\anims\idle3"
	fps 30
	loop
}
$sequence "attack1hit" {
	"..\anims\attack1hit"
	fps 70
}
$sequence "attack2hit" {
	"..\anims\attack2hit"
	fps 70
}
$sequence "attack3hit" {
	"..\anims\attack3hit"
	fps 70
}
$sequence "attack1miss" {
	"..\anims\attack1miss"
	fps 70
}
$sequence "attack2miss" {
	"..\anims\attack2miss"
	fps 70
}
$sequence "attack3miss" {
	"..\anims\attack3miss"
	fps 70
}
$sequence "draw" {
	"..\anims\draw"
	fps 24
}
$sequence "holster" {
	"..\anims\holster"
	fps 24
}

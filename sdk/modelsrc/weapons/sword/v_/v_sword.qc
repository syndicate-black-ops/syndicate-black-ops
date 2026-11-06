$modelname "v_sword.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "epee_ref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -2.28 -1.54 -1.36 11.86 1.15 1.68
$hbox 0 "Bip01 R Arm2" -2.34 -1.6 -2 11.89 1.15 1.13
$hbox 0 "Bone01" -3.75 -3.68 -6.87 72.800003 0.72 4.24

$sequence "idle1" {
	"..\anims\idle1"
	fps 22
	loop
}
$sequence "draw" {
	"..\anims\draw"
	fps 22
}
$sequence "holster" {
	"..\anims\holster"
	fps 22
}
$sequence "attack1" {
	"..\anims\attack1"
	fps 22
}
$sequence "attack1miss" {
	"..\anims\attack1miss"
	fps 22
}
$sequence "attack2" {
	"..\anims\attack2"
	fps 22
}
$sequence "attack2hit" {
	"..\anims\attack2hit"
	fps 22
}
$sequence "attack3" {
	"..\anims\attack3"
	fps 22
}
$sequence "attack3hit" {
	"..\anims\attack3hit"
	fps 22
}
$sequence "idle2" {
	"..\anims\idle2"
	fps 22
	loop
}
$sequence "idle3" {
	"..\anims\idle3"
	fps 22
	loop
}

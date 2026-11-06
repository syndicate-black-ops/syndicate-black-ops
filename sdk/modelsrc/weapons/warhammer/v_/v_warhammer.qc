$modelname "v_warhammer.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "warhammer_ref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -2.28 -1.54 -1.36 11.86 1.15 1.68
$hbox 0 "Bip01 R Arm2" -2.34 -1.6 -2 11.89 1.15 1.13
$hbox 0 "Bip01 R Hand" -0.8 -0.4 -13.929707 4.26 1.154849 1.73
$hbox 0 "Bone01" -26.209999 -4.01 -5.8 43.02 12.6 4.66
$hbox 0 "Bone08" -1.76 -11.04 -5.4 7.29 4.05 2.98

$sequence "idle1" {
	"..\anims\idle1"
	fps 22
	loop
}
$sequence "draw" {
	"..\anims\draw"
	fps 26
}
$sequence "holster" {
	"..\anims\holster"
	fps 22
}
$sequence "attack1" {
	"..\anims\attack1"
	fps 26
}
$sequence "attack1miss" {
	"..\anims\attack1miss"
	fps 26
}
$sequence "attack2" {
	"..\anims\attack2"
	fps 26
}
$sequence "attack2hit" {
	"..\anims\attack2hit"
	fps 26
}
$sequence "attack3" {
	"..\anims\attack3"
	fps 32
}
$sequence "attack3hit" {
	"..\anims\attack3hit"
	fps 32
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

$modelname "v_satchel_radio.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "satchel_radio_hand"
}

$bodygroup "body"
{
	studio "satchel_radio_detonator"
}

$flags 0

$texrendermode "xbow_sleeve.bmp" chrome
$texrendermode "glove_handpak.bmp.bmp" chrome
$texrendermode "gloved_knuckle.bmp" chrome

$texturegroup "skinfamilies"
{
	{ "det_front.bmp" }
	{ "det_front_lh.bmp" }
}

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 R Arm2" 0 -1.81 -2.52 11.368423 3.36 2.09
$hbox 0 "Bip01 R Hand" -0.19 -0.93 -1.75 3.89 2.409614 1.94
$hbox 0 "Dummy22" -1.91 -0.76 -2.02 1.71 0.9 9.85

$sequence "idle1" {
	"..\anims\idle1"
	fps 30
	loop
}
$sequence "fidget1" {
	"..\anims\fidget1"
	fps 30
}
$sequence "draw" {
	"..\anims\draw"
	fps 30
}
$sequence "fire" {
	"..\anims\fire"
	fps 30
}
$sequence "holster" {
	"..\anims\holster"
	fps 30
}

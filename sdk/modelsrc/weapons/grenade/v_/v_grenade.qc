$modelname "v_grenade.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "grenade"
}

$bodygroup "body"
{
	studio "grenade_hand"
}

$flags 0

$texrendermode "xbow_sleeve.bmp" chrome
$texrendermode "gloved_knuckle.bmp" chrome
$texrendermode "glove_handpak.bmp.bmp" chrome

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 R Arm1" 0 -2.81 -3.31 12.35 3.49 2.62
$hbox 0 "Bip01 R Arm2" 0 -2.61 -3.14 11.36842 3.21 2.34
$hbox 0 "Bip01 R Hand" 0 -0.87 -1.79 3.89 1.03 1.94
$hbox 0 "Dummy22" -1.01 -1.22 -2.13 1.62 1.92 3.11
$hbox 0 "Bip01 L Arm2" 0 -2.28 -2.12 11.68 2.74 2.38
$hbox 0 "Bip01 L Hand" 0 -0.87 -1.94 3.89 1.03 1.79

$sequence "idle" {
	"..\anims\idle"
	fps 30
	loop
}
$sequence "fidget" {
	"..\anims\fidget"
	fps 30
	loop
}
$sequence "pinpull" {
	"..\anims\pinpull"
	fps 45
}
$sequence "throw1" {
	"..\anims\throw1"
	fps 13
}
$sequence "throw2" {
	"..\anims\throw2"
	fps 20
}
$sequence "throw3" {
	"..\anims\throw3"
	fps 30
}
$sequence "holster" {
	"..\anims\holster"
	fps 30
}
$sequence "draw" {
	"..\anims\draw"
	fps 30
}

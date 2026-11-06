$modelname "v_rc28.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "testref"
}

$flags 0

$attachment 0 "Bone04" 0 0 0

$texturegroup "skinfamilies"
{
	{ "rccote.bmp" }
	{ "rccote_lh.bmp" }
}

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" 0 -2.26 -2.53 11.9 2.15 2.53
$hbox 0 "Bip01 L Hand" -0.38 -0.54 -1.57 4.23 1.35 1.63
$hbox 0 "Bip01 R Arm2" 0 -2.23 -2.79 11.92 2.14 2.27
$hbox 0 "Bip01 R Hand" -0.34 -0.4 -1.993293 4.738753 0.900783 1.74
$hbox 0 "Bone01" -0.63 -1.56 -0.52 4.625988 0.59 0.69
$hbox 0 "Bone03" -5.03 -10.531021 -0.86 2.47 2.11 1.64
$hbox 0 "Bone05" -2.093188 0 -1.115427 0.217949 11.537338 0
$hbox 0 "Bone09" -5.32 -2.42 -0.71 2.4 0 0.79

$sequence "idle1" {
	"..\anims\idle1"
	fps 16
}
$sequence "shoot1" {
	"..\anims\shoot1"
	{ event 5001 0 "12" }
	fps 90
}
$sequence "shoot2" {
	"..\anims\shoot2"
	{ event 5001 0 "12" }
	fps 40
}
$sequence "shoot3" {
	"..\anims\shoot3"
	{ event 5001 0 "12" }
	fps 90
}
$sequence "shoot_empty" {
	"..\anims\shoot_empty"
	{ event 5001 0 "12" }
	fps 34
}
$sequence "reload" {
	"..\anims\reload"
	fps 25
}
$sequence "deagle_draw" {
	"..\anims\deagle_draw"
	fps 35
}

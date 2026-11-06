$modelname "v_federal.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "far2base"
}

$flags 0

$attachment 0 "Bone14" 0 0 0

$texturegroup "skinfamilies"
{
	{ "skin.bmp" }
	{ "skin_lh.bmp" }
}

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 L Arm2" -3.15 -1.73 -1.56 12.99 1.34 1.72
$hbox 0 "Bip01 R Arm2" -3.01 -1.76 -2.18 12.79 1.31 1.29
$hbox 0 "Bip01 R Hand" -0.8 -0.4 -1.805943 4.388212 0.694793 1.73
$hbox 0 "Bone01" 0 -1.010392 -1.045932 4.733747 0 0
$hbox 0 "Bone02" -0.53 -0.351571 -0.104424 2.130588 11.38 5.29
$hbox 0 "Bone09" -0.774391 -0.570956 -7.55699 0.352721 4.404325 0
$hbox 0 "Bone13" 0 -6.026941 -2.522429 1.09679 0 0
$hbox 0 "Bone14" -10.58 -0.21 -0.8 1.63 18.74 8.52
$hbox 0 "Chargeur" -1.38 -3.08 -0.56 2.45 3.44 0.57

$sequence "idle1" {
	"..\anims\idle1"
	fps 16
}
$sequence "reload" {
	"..\anims\reload"
	fps 20
}
$sequence "draw" {
	"..\anims\draw"
	fps 7
}
$sequence "shoot1" {
	"..\anims\shoot1"
	{ event 5001 0 "21" }
	fps 9
}

$modelname "p_fa18.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_fa18"
}

$flags 0

$texrendermode "fa18p_02.bmp" chrome

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 R Hand" -7.909166 0 0 0 1.556563 5.26035
$hbox 0 "Cylinder04" -1.23 -9.94 -0.55 33.75 1.72 2.69

$sequence "idle" {
	"..\anims\idle"
	fps 30
}

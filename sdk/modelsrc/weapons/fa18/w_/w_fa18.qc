$modelname "w_fa18.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "w_fa18"
}

$flags 0

$texrendermode "fa18p_02.bmp" chrome

$sequence "idle" {
	"..\anims\idle"
	fps 30
}

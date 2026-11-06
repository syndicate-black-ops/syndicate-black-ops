$modelname "p_katana.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_troncref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -3.112024 0 -4.140396 0 1.517254 4.34191
$hbox 0 "Bip01 L Hand" 0 -1.79 -43.18 7.52 2.31 5.05
$hbox 0 "Bip01 R Hand" -4.83 -1.86 -4.51 5.01 2.25 43.099998

$sequence "idle" {
	"..\anims\idle"
	fps 30
}

$modelname "p_rc28.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_rc28"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869898 0 -4.140396 0 1.399663 0
$hbox 0 "Bip01 R Hand" -0.11 -0.86 -2.58 14.4 2.19 10.21

$sequence "idle" {
	"..\anims\idle"
	fps 30
}

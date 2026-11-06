$modelname "p_federal.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_far2"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869898 0 -4.140396 0 1.399663 0
$hbox 0 "Bip01 R Hand" -2.65 -0.6 -3.84 23.040001 3.61 7.85

$sequence "idle" {
	"..\anims\idle"
	fps 30
}

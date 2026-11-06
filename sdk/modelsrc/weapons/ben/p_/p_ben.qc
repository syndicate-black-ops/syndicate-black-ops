$modelname "p_ben.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_pepomref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869898 0 -4.140396 0 1.399663 0
$hbox 0 "Bip01 R Hand" -7.88 -1.3 -2.66 29.99 3.18 5.62

$sequence "idle" {
	"..\anims\idle"
	fps 9
	loop
}

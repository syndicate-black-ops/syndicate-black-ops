$modelname "p_canon.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_terrorref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869898 0 -4.140396 0 1.399663 0
$hbox 0 "Bip01 R Hand" -2.26 0 -3.17 21.780001 4.38 7.36

$sequence "spaidle" {
	"..\anims\spaidle"
	fps 15
	loop
}

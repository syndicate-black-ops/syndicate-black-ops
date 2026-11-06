$modelname "p_pm66.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_pmnew"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869898 0 -4.140396 0 1.399663 0
$hbox 0 "Bip01 R Hand" -2.78 0 -2.69 12.29 2.19 6.1

$sequence "spaidle" {
	"..\anims\spaidle"
	fps 15
	loop
}

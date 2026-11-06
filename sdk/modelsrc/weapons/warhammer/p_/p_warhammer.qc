$modelname "p_warhammer.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "p_warhammer"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869898 0 -4.140396 0 1.399663 0
$hbox 0 "Bip01 R Hand" -16.99 -7.4 -7.85 8.08 2.44 56.02

$sequence "spaidle" {
	"..\anims\spaidle"
	fps 15
	loop
}

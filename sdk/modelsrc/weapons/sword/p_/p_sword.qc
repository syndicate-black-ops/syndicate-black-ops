$modelname "p_sword.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "p_swordref"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.869898 0 -4.140396 0 1.399663 0
$hbox 0 "Bip01 R Hand" 0 -4.05 -5.03 15.02 4.72 57.419998

$sequence "idle" {
	"..\anims\idle"
	ACT_IDLE 1
	fps 30
	loop
}

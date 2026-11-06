$modelname "p_medikit.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "p_medkit"
}

$flags 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Neck" -2.085405 0 0 0 2.764391 4.341913
$hbox 0 "Box02" -5.72 -0.88 -6.48 5.75 2.5 4.98

$sequence "idle" {
	"..\anims\idle"
	fps 30
}

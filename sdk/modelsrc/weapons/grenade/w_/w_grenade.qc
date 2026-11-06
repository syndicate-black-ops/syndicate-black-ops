$modelname "w_grenade.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "w_grenade"
}

$flags 0

$sequence "idle" {
	"..\anims\idle"
	fps 30
}
$sequence "roll1" {
	"..\anims\roll1"
	fps 15
	loop
}
$sequence "roll2" {
	"..\anims\roll2"
	fps 15
	loop
}
$sequence "toss" {
	"..\anims\toss"
	fps 10
	loop
}
$sequence "toss2" {
	"..\anims\toss2"
	fps 10
	loop
}
$sequence "toss3" {
	"..\anims\toss3"
	fps 10
	loop
}
$sequence "toss4" {
	"..\anims\toss4"
	fps 10
	loop
}

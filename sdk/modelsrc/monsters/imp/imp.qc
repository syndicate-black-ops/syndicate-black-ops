$modelname "imp.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "imp"
}

$flags 0

$eyeposition 0 0 60

$cbox 0 0 0 0 0 0

$bbox -16 -16 0 16 16 72

$hbox 3 "Bip02 Pelvis" -0.000004 -1.91 -7.74 3.6 6.94 7.61
$hbox 6 "Bip02 L Leg" -0.11 -0.2 -3.91 16.904898 12.07 2.71
$hbox 6 "Bip02 L Leg1" -4.67 0 -4.23 16.59754 7.92 1.57
$hbox 6 "Bip02 L Foot" -0.25 0 -4.42 3.48 14.13 0.56
$hbox 7 "Bip02 R Leg" -0.06 -2.99 -3.07 16.904898 8.09 4.75
$hbox 7 "Bip02 R Leg1" -2.58 -5.83 -1.25 16.59754 2.39 4.04
$hbox 7 "Bip02 R Foot" -0.3 -4.4 -0.5 3.45 5.81 5.04
$hbox 3 "Bip02 Spine" 0 -2.28 -5.98 5.318531 5.91 6.3
$hbox 2 "Bip02 Spine1" 0 -2.86 -4.97 7.88 7.09 5.16
$hbox 2 "Bip02 Spine2" 0 -12.37 -5.59 13.77 12.29 6.1
$hbox 4 "Bip02 L Arm" 0 -5.24 0 7.28 6.37 10.44
$hbox 4 "Bip02 L Arm1" 0 -2.65 -0.45 13.96 7.31 3.64
$hbox 4 "Bip02 L Arm2" -1.19 -1.29 -0.77 10.954373 4.03 3.93
$hbox 4 "Bip02 L Hand" -2.56 -3.82 -0.66 3.09 0.97 3.71
$hbox 5 "Bip02 R Arm1" -8.93 -3.17 -5.42 13.28 7.31 1.87
$hbox 5 "Bip02 R Arm2" -0.91 -3.12 -3.15 10.954377 2.79 1.99
$hbox 5 "Bip02 R Hand" -3.23 -2.8 -0.18 2.75 0.92 5.63
$hbox 1 "Bip02 Head" 4 -3.07 -2.85 12.96 5.68 3.25

$controller 0 "Bip02 Head" XR -60 60

$sequence "walk" {
	"..\anims\walk"
	ACT_WALK 1
	fps 30
	loop
	LX
}
$sequence "run" {
	"..\anims\run"
	ACT_RUN 1
	fps 30
	loop
	LX
}
$sequence "suprisedhop" {
	"..\anims\suprisedhop"
	ACT_HOP 1
	{ event 5 11 }
	fps 42
}
$sequence "flinchs" {
	"..\anims\flinchs"
	ACT_SMALL_FLINCH 1
	fps 30
}
$sequence "flinchb" {
	"..\anims\flinchb"
	ACT_BIG_FLINCH 1
	fps 30
}
$sequence "turnLeft" {
	"..\anims\turnleft"
	ACT_TURN_LEFT 1
	fps 30
}
$sequence "turnRight" {
	"..\anims\turnright"
	ACT_TURN_RIGHT 1
	fps 30
}
$sequence "idle" {
	"..\anims\idle"
	ACT_IDLE 1
	fps 30
	loop
}
$sequence "whip" {
	"..\anims\whip"
	ACT_FEAR_DISPLAY 1
	fps 15
}
$sequence "bite" {
	"..\anims\bite"
	ACT_MELEE_ATTACK2 1
	{ event 2 7 "{" }
	{ event 6 10 }
	fps 30
}
$sequence "range" {
	"..\anims\range"
	ACT_RANGE_ATTACK1 1
	{ event 1 11 }
	fps 30
}
$sequence "look" {
	"..\anims\look"
	ACT_INSPECT_WALL 1
	{ event 3 10 }
	{ event 3 20 }
	fps 20
}
$sequence "seecrab" {
	"..\anims\seecrab"
	ACT_EXCITED 1
	fps 30
}
$sequence "eat" {
	"..\anims\eat"
	ACT_EAT 1
	fps 30
	loop
}
$sequence "inspectdown" {
	"..\anims\inspectdown"
	ACT_INSPECT_FLOOR 1
	fps 30
}
$sequence "sniff" {
	"..\anims\sniff"
	ACT_DETECT_SCENT 1
	fps 15
}
$sequence "die" {
	"..\anims\die"
	ACT_DIEFORWARD 1
	{ event 2002 2 }
	fps 15
}
$sequence "die1" {
	"..\anims\die1"
	ACT_DIESIMPLE 1
	{ event 2002 12 }
	fps 20
}
$sequence "bulljump" {
	"..\anims\bulljump"
	fps 25
	AX AY AZR
}
$sequence "draggruntidle" {
	"..\anims\draggruntidle"
	fps 14
}
$sequence "draggrunt" {
	"..\anims\draggrunt"
	{ event 1004 1 "bullchicken/bc_idle3.wav" }
	{ event 1004 8 "bullchicken/bc_idle1.wav" }
	fps 10
}
$sequence "scare" {
	"..\anims\scare"
	{ event 1004 8 "bullchicken/bc_attackgrowl3.wav" }
	fps 20
}
$sequence "squidfallidle" {
	"..\anims\squidfallidle"
	fps 33
	loop
}
$sequence "squidfall" {
	"..\anims\squidfall"
	{ event 1000 1 }
	{ event 1008 3 "bullchicken/bc_attackgrowl2.wav" }
	{ event 1008 33 "bullchicken/bc_attackgrowl3.wav" }
	{ event 1003 45 "bssplat" }
	{ event 1008 45 "debris/bustflesh2.wav" }
	fps 32
}

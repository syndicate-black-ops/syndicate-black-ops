$modelname "zombie.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "zombiewte"
	studio "zombieblk"
}

$flags 0

$eyeposition 0 0 55

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 0 "Bip01 Pelvis" -0.96 -1.5 -4.91 4.45 9.09 5.03
$hbox 6 "Bip01 L Leg" 0 -0.71 -2.19 16.078632 6.24 3.47
$hbox 6 "Bip01 L Leg1" -6.4 -1.11 -2.84 16.078634 5.14 1.99
$hbox 6 "Bip01 L Foot" -4.47 0 -3.39 0.88 11.36 0.8
$hbox 7 "Bip01 R Leg" -0.33 -3.41 -3.63 16.078632 5.69 2.75
$hbox 7 "Bip01 R Leg1" -4.51 -5.92 -1.81 16.078634 0.14 2.48
$hbox 7 "Bip01 R Foot" -4.5 -3.65 -0.85 0.86 4.62 3.63
$hbox 0 "Bip01 Spine" 0 -0.003328 -3.4 8.18 7.32 3.1
$hbox 3 "Bip01 Spine1" 0 -4.08 -3.96 4.180562 0 4.11
$hbox 2 "Bip01 Spine2" 0 -5.91 -4.59 5.36 3.93 5.6
$hbox 0 "Bip01 Spine3" 0 -6.01 -2.38 6.7 1.44 2.51
$hbox 1 "Bip01 Neck" 0 -5.44 -2.963063 8.07 1.89 2.963063
$hbox 4 "Bip01 L Arm" 0 -0.92 -1.81 5.35 4.48 0
$hbox 4 "Bip01 L Arm1" 0 -6.85 -4.44 23.040001 3.52 0
$hbox 5 "Bip01 R Arm" 0 0 0 4.93 5.8 2.37
$hbox 5 "Bip01 R Arm1" 0 -10 0 20.549999 5.13 5.45

$sequence "idle1" {
	"..\anims\idle1"
	ACT_IDLE 4
	fps 15
	loop
}
$sequence "turnleft" {
	"..\anims\turnleft"
	ACT_TURN_LEFT 1
	fps 20
}
$sequence "turnright" {
	"..\anims\turnright"
	ACT_TURN_RIGHT 1
	fps 20
}
$sequence "flinchsmall" {
	"..\anims\flinchsmall"
	ACT_SMALL_FLINCH 5
	fps 22
}
$sequence "flinch" {
	"..\anims\flinch"
	ACT_SMALL_FLINCH 1
	fps 22
}
$sequence "bigflinch" {
	"..\anims\bigflinch"
	ACT_BIG_FLINCH 1
	fps 20
}
$sequence "falling" {
	"..\anims\falling"
	ACT_FALL 1
	fps 26
	loop
}
$sequence "attack1" {
	"..\anims\attack1"
	ACT_MELEE_ATTACK1 5
	{ event 1 10 }
	{ event 2 19 }
	fps 15
}
$sequence "attack2" {
	"..\anims\attack2"
	ACT_MELEE_ATTACK1 1
	{ event 3 6 }
	fps 15
}
$sequence "walk" {
	"..\anims\walk"
	ACT_WALK 1
	{ event 1004 2 "common/npc_step1.wav" }
	{ event 1004 12 "common/npc_step3.wav" }
	{ event 1004 22 "common/npc_step2.wav" }
	{ event 1004 31 "common/npc_step4.wav" }
	{ event 1004 41 "common/npc_step1.wav" }
	{ event 1004 51 "common/npc_step3.wav" }
	fps 22
	loop
	LX
}
$sequence "laflinch" {
	"..\anims\laflinch"
	ACT_FLINCH_LEFTARM 1
	fps 15
}
$sequence "raflinch" {
	"..\anims\raflinch"
	ACT_FLINCH_RIGHTARM 1
	fps 15
}
$sequence "llflinch" {
	"..\anims\llflinch"
	ACT_FLINCH_LEFTLEG 1
	fps 15
}
$sequence "rlflinch" {
	"..\anims\rlflinch"
	ACT_FLINCH_RIGHTLEG 1
	fps 15
}
$sequence "dieheadshot" {
	"..\anims\dieheadshot"
	ACT_DIE_HEADSHOT 1
	{ event 2001 19 }
	fps 20
	X
}
$sequence "dieheadshot2" {
	"..\anims\dieheadshot2"
	ACT_DIE_GUTSHOT 1
	{ event 2001 18 }
	fps 22
}
$sequence "diesimple" {
	"..\anims\diesimple"
	ACT_DIESIMPLE 1
	{ event 2001 13 }
	fps 20
	X
}
$sequence "diebackward" {
	"..\anims\diebackward"
	ACT_DIEBACKWARD 1
	{ event 2001 10 }
	fps 18
}
$sequence "dieforward" {
	"..\anims\dieforward"
	ACT_DIEFORWARD 1
	{ event 2001 21 }
	fps 18
	X
}
$sequence "getup" {
	"..\anims\getup"
	fps 15
}
$sequence "pause" {
	"..\anims\pause"
	fps 15
}
$sequence "busting_through_wall" {
	"..\anims\busting_through_wall"
	fps 20
}
$sequence "kick_punch_wall" {
	"..\anims\kick_punch_wall"
	fps 14
}
$sequence "bust_window" {
	"..\anims\bust_window"
	fps 26
}
$sequence "soda" {
	"..\anims\soda"
	{ event 1004 63 "*buttons/latchunlocked1.wav" }
	fps 26
}
$sequence "slideidle" {
	"..\anims\slideidle"
	fps 16
	loop
}
$sequence "slidewall" {
	"..\anims\slidewall"
	{ event 1000 1 }
	fps 18
}
$sequence "ventclimbidle" {
	"..\anims\ventclimbidle"
	fps 15
	loop
}
$sequence "ventclimb" {
	"..\anims\ventclimb"
	fps 20
}
$sequence "deadidle" {
	"..\anims\deadidle"
	fps 14
	loop
}
$sequence "deadwall" {
	"..\anims\deadwall"
	fps 16
}
$sequence "freakdie" {
	"..\anims\freakdie"
	{ event 1000 0 }
	fps 12
}
$sequence "freak" {
	"..\anims\freak"
	{ event 1003 2 "freak" }
	fps 16
	loop
}
$sequence "eatbodytable" {
	"..\anims\eatbodytable"
	fps 20
	loop
}
$sequence "eatbody" {
	"..\anims\eatbody"
	fps 15
	loop
}
$sequence "eatbodystand" {
	"..\anims\eatbodystand"
	fps 30
}
$sequence "ripdoor" {
	"..\anims\ripdoor"
	{ event 1008 1 "*debris/bustcrate1.wav" }
	fps 20
}
$sequence "zombie_pull_scientist" {
	"..\anims\zombie_pull_scientist"
	fps 17
}
$sequence "zombie_eating" {
	"..\anims\zombie_eating"
	fps 15
	loop
	AX AY AZR
}
$sequence "eat_to_stand" {
	"..\anims\eat_to_stand"
	fps 15
	AX AY AZR
}
$sequence "vent_zidle" {
	"..\anims\vent_zidle"
	fps 18
}
$sequence "vent_c1a3" {
	"..\anims\vent_c1a3"
	{ event 1000 1 }
	fps 18
	AX AY AZR
}
$sequence "haulzombie" {
	"..\anims\haulzombie"
	fps 18
	AX AY AZR
}
$sequence "c2a3_snack_getup" {
	"..\anims\c2a3_snack_getup"
	fps 12
	AX AY AZR
}

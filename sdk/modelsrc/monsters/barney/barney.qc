$modelname "barney.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "studio"
{
	studio "barney-x_template_biped1"
}

$bodygroup "gun"
{
	studio "barney-x_template_biped(gun-hlstr)1"
	studio "barney-x_template_biped(gun)1"
	blank
}

$flags 0

$eyeposition 0 0 63

$attachment 0 "Bip01 R Hand" 15 3.5 5

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 3 "Bip01 Pelvis" -9.89 -5.66 -8.6 2.94 6.52 6.79
$hbox 6 "Bip01 L Leg" 0 -5.92 -4 19.370001 3.78 3.73
$hbox 6 "Bip01 L Leg1" 0 -5.69 -3.62 18.83 3.07 2.94
$hbox 6 "Bip01 L Foot" 0 -2.99 -4.04 6.42 8.68 1.61
$hbox 7 "Bip01 R Leg" 0 -5.93 -3.87 19.27 3.75 3.79
$hbox 7 "Bip01 R Leg1" -0.33 -5.73 -2.98 18.950001 2.96 3.19
$hbox 7 "Bip01 R Foot" 0 -3.24 -2.32 6.21 8.44 3.28
$hbox 3 "Bip01 Spine" 0 -4.89 -6.97 8.37 8 6.73
$hbox 2 "Bip01 Spine2" 0 -3.46 -8.32 6.11 8 7.53
$hbox 2 "Bip01 Spine3" -1 -5.99 -8.22 5.96 8 8.48
$hbox 4 "Bip01 L Arm" -0.4 -4.18 0 7.2 4.75 4.2
$hbox 4 "Bip01 L Arm1" -2.52 -4.57 -3.92 9.86 2.28 3.43
$hbox 4 "Bip01 L Arm2" 0 -2.85 -2.08 11.9 2.85 2.75
$hbox 4 "Bip01 L Hand" 0 -1.32 -2.29 4.66 2.6 1.65
$hbox 5 "Bip01 R Arm" 0 -4.15 -4.19 7.53 4.12 4.24
$hbox 5 "Bip01 R Arm1" -2.69 -4.65 -3.42 9.47 2.28 3.76
$hbox 5 "Bip01 R Arm2" 0 -2.73 -2.56 11 3.14 2.19
$hbox 5 "Bip01 R Hand" 0 -1.32 -2.91 10.38 3.23 4
$hbox 1 "Bip01 Head" -0.66 2.25 -4.24 6 7.97 3.63
$hbox 10 "Bip01 Head" -0.66 -4.25 -5.24 6 2.25 4.63
$hbox 10 "Bip01 Head" 6 -4.25 -5.24 11.19 7.97 4.63

$controller 0 "Bip01 Head" XR -60 60
$controller 4 "Bone05" ZR 0 45

$sequence "idle1" {
	"..\anims\idle1"
	ACT_IDLE 50
	fps 15
	loop
}
$sequence "idle2" {
	"..\anims\idle2"
	ACT_IDLE 1
	fps 15
}
$sequence "idle3" {
	"..\anims\idle3"
	ACT_IDLE 1
	fps 15
}
$sequence "idle4" {
	"..\anims\idle4"
	ACT_IDLE 10
	fps 12
}
$sequence "walk" {
	"..\anims\walk"
	ACT_WALK 1
	{ event 1004 3 "common/npc_step1.wav" }
	{ event 1004 18 "common/npc_step3.wav" }
	fps 30
	loop
	LX
}
$sequence "run" {
	"..\anims\run"
	ACT_RUN 1
	{ event 1004 5 "common/npc_step2.wav" }
	{ event 1004 13 "common/npc_step4.wav" }
	fps 25
	loop
	LX
}
$sequence "shootgun" {
	"..\anims\shootgun_blend01"
	"..\anims\shootgun_blend02"
	ACT_RANGE_ATTACK1 1
	blend XR -50 50
	{ event 5001 0 "21" }
	{ event 3 0 }
	fps 25
}
$sequence "shootgun2" {
	"..\anims\shootgun2_blend01"
	"..\anims\shootgun2_blend02"
	ACT_RANGE_ATTACK1 2
	blend XR -50 50
	{ event 5001 0 "21" }
	{ event 3 0 }
	fps 40
}
$sequence "draw" {
	"..\anims\draw"
	ACT_ARM 1
	{ event 2 7 }
	fps 20
}
$sequence "disarm" {
	"..\anims\disarm"
	ACT_DISARM 1
	{ event 4 15 }
	fps 10
}
$sequence "reload" {
	"..\anims\reload"
	ACT_RELOAD 1
	fps 15
}
$sequence "turnleft" {
	"..\anims\turnleft"
	ACT_TURN_LEFT 1
	fps 15
}
$sequence "turnright" {
	"..\anims\turnright"
	ACT_TURN_RIGHT 1
	fps 15
}
$sequence "laflinch" {
	"..\anims\laflinch"
	ACT_FLINCH_LEFTARM 1
	fps 16
}
$sequence "raflinch" {
	"..\anims\raflinch"
	ACT_FLINCH_RIGHTARM 1
	fps 16
}
$sequence "llflinch" {
	"..\anims\llflinch"
	ACT_FLINCH_LEFTLEG 1
	fps 20
}
$sequence "rlflinch" {
	"..\anims\rlflinch"
	ACT_FLINCH_RIGHTLEG 1
	fps 20
}
$sequence "smlflinch" {
	"..\anims\smlflinch"
	ACT_SMALL_FLINCH 1
	fps 30
}
$sequence "cower_stand" {
	"..\anims\cower_stand"
	fps 14
}
$sequence "locked_door" {
	"..\anims\locked_door"
	fps 28
	loop
}
$sequence "fall_loop" {
	"..\anims\fall_loop"
	fps 28
	loop
}
$sequence "barn_wave" {
	"..\anims\barn_wave"
	fps 22
}
$sequence "beat_grunt" {
	"..\anims\beat_grunt"
	fps 14
}
$sequence "beat_gruntidle" {
	"..\anims\beat_gruntidle"
	fps 14
	loop
}
$sequence "flashlight" {
	"..\anims\flashlight"
	{ event 1004 5 "*buttons/latchunlocked2.wav" }
	fps 14
	loop
}
$sequence "diesimple" {
	"..\anims\diesimple"
	ACT_DIE_HEADSHOT 1
	{ event 2001 5 }
	fps 15
	X
}
$sequence "dieviolent" {
	"..\anims\dieviolent"
	ACT_DIE_HEADSHOT 1
	{ event 2001 10 }
	fps 20
	X
}
$sequence "diegutshot" {
	"..\anims\diegutshot"
	ACT_DIE_GUTSHOT 1
	{ event 2001 33 }
	fps 15
}
$sequence "dieforward" {
	"..\anims\dieforward"
	ACT_DIEFORWARD 1
	{ event 2001 8 }
	fps 18
	X
}
$sequence "diebackward" {
	"..\anims\diebackward"
	ACT_DIEBACKWARD 1
	{ event 2001 19 }
	fps 15
	X
}
$sequence "diecrump" {
	"..\anims\diecrump"
	ACT_DIESIMPLE 2
	{ event 2001 13 }
	fps 18
	X
}
$sequence "barnaclehit" {
	"..\anims\barnaclehit"
	ACT_BARNACLE_HIT 1
	fps 15
}
$sequence "barnaclepull" {
	"..\anims\barnaclepull"
	ACT_BARNACLE_PULL 1
	fps 45
	loop
}
$sequence "barnaclecrunch" {
	"..\anims\barnaclecrunch"
	ACT_BARNACLE_CHOMP 1
	fps 5
}
$sequence "barnaclechew" {
	"..\anims\barnaclechew"
	ACT_BARNACLE_CHEW 1
	fps 5
	loop
}
$sequence "lying_on_back" {
	"..\anims\lying_on_back"
	fps 1
}
$sequence "lying_on_side" {
	"..\anims\lying_on_side"
	fps 1
}
$sequence "lying_on_stomach" {
	"..\anims\lying_on_stomach"
	fps 1
}
$sequence "stuffed_in_vent" {
	"..\anims\stuffed_in_vent"
	fps 1
}
$sequence "standing_idle" {
	"..\anims\standing_idle"
	fps 15
	loop
}
$sequence "cprbarney" {
	"..\anims\cprbarney"
	fps 20
	loop
}
$sequence "cprbarneyrevive" {
	"..\anims\cprbarneyrevive"
	fps 20
}
$sequence "barney_dragvent" {
	"..\anims\barney_dragvent"
	{ event 1000 1 }
	fps 15
}
$sequence "dying_barney" {
	"..\anims\dying_barney"
	{ event 1000 1 }
	fps 18
}
$sequence "dying_barneyidle" {
	"..\anims\dying_barneyidle"
	fps 12
	loop
}
$sequence "dying_friend" {
	"..\anims\dying_friend"
	fps 18
}
$sequence "dying_friendidle" {
	"..\anims\dying_friendidle"
	fps 12
	loop
}
$sequence "c1a3_bidle" {
	"..\anims\c1a3_bidle"
	fps 3
	AX AY AZR
}
$sequence "c1a3_ventb" {
	"..\anims\c1a3_ventb"
	{ event 1000 1 }
	fps 18
	AX AY AZR
}
$sequence "c1a3_emergeidle" {
	"..\anims\c1a3_emergeidle"
	fps 16
	loop
	AX AY AZR
}
$sequence "c1a3_emerge" {
	"..\anims\c1a3_emerge"
	{ event 1000 1 }
	fps 16
	AX AY AZR
}
$sequence "haulbarney" {
	"..\anims\haulbarney"
	{ event 1000 1 }
	fps 18
	AX AY AZR
}
$sequence "intropush" {
	"..\anims\intropush"
	{ event 1004 15 "*buttons/blip1.wav" }
	{ event 1004 24 "*buttons/blip1.wav" }
	{ event 1004 34 "*buttons/blip1.wav" }
	{ event 1004 44 "*buttons/blip1.wav" }
	fps 16
}
$sequence "fence" {
	"..\anims\fence"
	{ event 1000 70 }
	{ event 1003 70 "lightning" }
	{ event 1001 70 }
	{ event 1004 72 "weapons/electro4.wav" }
	{ event 1008 100 "barney/ba_pain2.wav" }
	fps 24
	AX AY AZR
}
$sequence "sit1" {
	"..\anims\sit1"
	{ event 1003 50 "introchair" }
	{ event 1003 102 "introchair" }
	fps 20
	loop
}
$sequence "almostidle" {
	"..\anims\almostidle"
	fps 16
	loop
}
$sequence "almost" {
	"..\anims\almost"
	{ event 1000 1 }
	fps 26
}
$sequence "laseridle" {
	"..\anims\laseridle"
	fps 6
}
$sequence "laser_top" {
	"..\anims\laser_top"
	{ event 1000 0 }
	fps 6
}
$sequence "laser_bottom" {
	"..\anims\laser_bottom"
	{ event 1000 0 }
	fps 6
}
$sequence "barneyfallidle" {
	"..\anims\barneyfallidle"
	fps 33
	loop
}
$sequence "barneyfall" {
	"..\anims\barneyfall"
	{ event 1000 1 }
	{ event 1008 2 "barney/ba_pain1.wav" }
	{ event 1008 24 "barney/ba_pain2.wav" }
	{ event 1003 55 "bsplat" }
	{ event 1008 55 "debris/bustflesh1.wav" }
	fps 32
}
$sequence "c3a2_draw" {
	"..\anims\c3a2_draw"
	{ event 2 13 }
	fps 16
}
$sequence "unlatch" {
	"..\anims\unlatch"
	{ event 1003 7 "barney_door_lock" }
	fps 12
}
$sequence "retina" {
	"..\anims\retina"
	fps 20
}
$sequence "relaxstand" {
	"..\anims\relaxstand"
	fps 10
	loop
}
$sequence "assassinated" {
	"..\anims\assassinated"
	{ event 1000 1 }
	{ event 1003 1 "blood1" }
	{ event 1003 2 "blood2" }
	fps 18
	X
}
$sequence "trackswitch" {
	"..\anims\trackswitch"
	{ event 1003 14 "trainarmswitch" }
	fps 22
}
$sequence "pepsiswing" {
	"..\anims\pepsiswing"
	{ event 1003 1 "die" }
	{ event 1003 8 "swing" }
	fps 25
}
$sequence "pepsipush" {
	"..\anims\pepsipush"
	{ event 1003 51 "topple" }
	{ event 1003 90 "horror" }
	fps 18
}
$sequence "buttonpush" {
	"..\anims\buttonpush"
	{ event 1003 7 "button" }
	fps 20
}

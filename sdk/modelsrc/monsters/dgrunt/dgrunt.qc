$modelname "dgrunt.mdl"
$cd ".\mesh"
$cdtexture ".\textures"
$scale 1.0

$bodygroup "body"
{
	studio "heavybody"
}

$bodygroup "heads"
{
	studio "heavyhead"
	studio "heavyhead_2"
	studio "heavyhead_3"
	studio "heavyhead_4"
}

$bodygroup "weapons"
{
	studio "gunreference"
	studio "gunreference(shotgun)"
	blank
	studio "gunreference(sniper)"
	studio "gunreference(faussegat)"
}

$flags 0

$eyeposition 0 0 73

$attachment 0 "Bip01 R Hand" 55 3 2
$attachment 1 "Bip01 R Hand" 0 0 0
$attachment 2 "Bip01 Head" 0 0 0

$cbox 0 0 0 0 0 0

$bbox 0 0 0 0 0 0

$hbox 3 "Bip01 Pelvis" -0.000007 -5.43 -6.48 7.08 7.45 7.28
$hbox 6 "Bip01 L Leg" 0 -6.02 -6.09 16.950001 5.82 5.71
$hbox 6 "Bip01 L Leg1" 0 -6.02 -4.63 22.146627 3.1 3.35
$hbox 6 "Bip01 L Foot" -1.28 -2.6 -2.89 2.65 10.1 1.74
$hbox 7 "Bip01 R Leg" 0 -4.88 -5.79 17.370001 5.83 5.32
$hbox 7 "Bip01 R Leg1" 0 -6.04 -4.12 22.146627 3.09 3.86
$hbox 7 "Bip01 R Foot" -1.29 -2.61 -2.5 2.64 10.09 2.13
$hbox 3 "Bip01 Spine" 0 -3.02 -7.46 14.37 8.54 8.24
$hbox 11 "Bip01 Neck" -5.55 -4.35 -8.49 4.35 7.32 9.37
$hbox 1 "Bip01 Head" 0 -2.88 -4.36 14.12 8.36 5.12
$hbox 4 "Bip01 L Arm" 0 -5.41 -4.11 15.12 3.89 4.43
$hbox 4 "Bip01 L Arm1" 0 -3.34 -3.96 9.341934 3.99 1.84
$hbox 4 "Bip01 L Arm2" -0.13 -2.03 -4.35 10 3.86 1.31
$hbox 4 "Bip01 L Hand" 0 -2.33 -3.32 13.47 1.99 2.68
$hbox 5 "Bip01 R Arm" 0 -5.4 -4.22 15.53 3.9 4.33
$hbox 5 "Bip01 R Arm1" -0.27 -3.33 -1.27 9.341938 4 4.33
$hbox 5 "Bip01 R Arm2" -0.04 -2.06 -1.43 9.8 3.84 4.24
$hbox 5 "Bip01 R Hand" 0 -2.31 -2.67 14.827692 1.79 3.33

$controller 0 "Bip01 Head" XR -70 70

$sequence "walk1" {
	"..\anims\walk1"
	ACT_WALK 1
	fps 40
	loop
	LX
}
$sequence "run" {
	"..\anims\run"
	ACT_RUN 1
	fps 40
	loop
	LX
}
$sequence "victorydance" {
	"..\anims\victorydance"
	ACT_VICTORY_DANCE 1
	fps 21
}
$sequence "cower" {
	"..\anims\cower"
	ACT_COWER 1
	fps 38
}
$sequence "smflinch" {
	"..\anims\smflinch"
	ACT_SMALL_FLINCH 5
	fps 20
}
$sequence "leftlegsmflinch" {
	"..\anims\leftlegsmflinch"
	ACT_FLINCH_LEFTLEG 1
	fps 22
}
$sequence "rightlegsmflinch" {
	"..\anims\rightlegsmflinch"
	ACT_FLINCH_RIGHTLEG 1
	fps 22
}
$sequence "rightarmflinch" {
	"..\anims\rightarmflinch"
	ACT_FLINCH_RIGHTARM 1
	fps 22
}
$sequence "leftarmflinch" {
	"..\anims\leftarmflinch"
	ACT_FLINCH_LEFTARM 1
	fps 22
}
$sequence "launchgrenade" {
	"..\anims\launchgrenade"
	ACT_RANGE_ATTACK2 1
	{ event 8 16 }
	fps 30
}
$sequence "throwgrenade" {
	"..\anims\throwgrenade"
	ACT_RANGE_ATTACK2 1
	{ event 7 36 }
	fps 30
}
$sequence "idle1" {
	"..\anims\idle1"
	ACT_IDLE 1
	fps 30
	loop
}
$sequence "idle2" {
	"..\anims\idle2"
	ACT_IDLE 1
	fps 10
	loop
}
$sequence "combatidle" {
	"..\anims\combatidle"
	ACT_IDLE_ANGRY 1
	fps 90
	loop
}
$sequence "frontkick" {
	"..\anims\frontkick"
	ACT_MELEE_ATTACK1 1
	{ event 3 14 }
	{ event 2010 14 }
	fps 35
}
$sequence "crouching_wait" {
	"..\anims\crouching_wait"
	ACT_CROUCH 0
	fps 200
}
$sequence "crouching_mp5" {
	"..\anims\crouching_mp5_blend01"
	"..\anims\crouching_mp5_blend02"
	ACT_RANGE_ATTACK1 1
	blend XR -45 45
	{ event 4 0 }
	{ event 5 2 }
	{ event 6 4 }
	{ event 5001 0 "50" }
	{ event 5001 2 "50" }
	{ event 5001 4 "50" }
	fps 50
	loop
}
$sequence "reload_mp5" {
	"..\anims\reload_mp5"
	ACT_RELOAD 1
	{ event 2 47 }
	fps 40
}
$sequence "crouching_shotgun" {
	"..\anims\crouching_shotgun_blend01"
	"..\anims\crouching_shotgun_blend02"
	ACT_RANGE_ATTACK1 1
	blend XR -45 45
	{ event 4 0 }
	{ event 5001 0 "51" }
	fps 20
}
$sequence "advance_signal" {
	"..\anims\advance_signal"
	ACT_SIGNAL1 1
	{ event 10 1 }
	fps 30
}
$sequence "flank_signal" {
	"..\anims\flank_signal"
	ACT_SIGNAL2 1
	fps 40
}
$sequence "retreat_signal" {
	"..\anims\retreat_signal"
	ACT_SIGNAL3 1
	fps 30
}
$sequence "drop_grenade" {
	"..\anims\drop_grenade"
	ACT_SPECIAL_ATTACK1 1
	{ event 9 22 }
	fps 40
}
$sequence "limpingwalk" {
	"..\anims\limpingwalk"
	ACT_WALK_HURT 1
	fps 40
	loop
	LX
}
$sequence "limpingrun" {
	"..\anims\limpingrun"
	ACT_RUN_HURT 1
	fps 40
	loop
	LX
}
$sequence "180L" {
	"..\anims\180l"
	ACT_TURN_LEFT 1
	fps 12
}
$sequence "180R" {
	"..\anims\180r"
	ACT_TURN_RIGHT 1
	fps 12
}
$sequence "strafeleft" {
	"..\anims\strafeleft"
	ACT_STRAFE_LEFT 1
	fps 30
	loop
	LX LY
}
$sequence "straferight" {
	"..\anims\straferight"
	ACT_STRAFE_RIGHT 1
	fps 30
	loop
	LX LY
}
$sequence "dieback1" {
	"..\anims\dieback1"
	ACT_DIEBACKWARD 1
	{ event 11 5 }
	{ event 2001 17 }
	fps 25
	X
}
$sequence "dieforward" {
	"..\anims\dieforward"
	ACT_DIEFORWARD 1
	{ event 11 3 }
	{ event 2001 10 }
	fps 26
	X
}
$sequence "diesimple" {
	"..\anims\diesimple"
	ACT_DIESIMPLE 1
	{ event 11 3 }
	{ event 2001 10 }
	fps 15
	X
}
$sequence "diebackwards" {
	"..\anims\diebackwards"
	ACT_DIEBACKWARD 1
	{ event 11 6 }
	{ event 2001 12 }
	fps 30
	X
}
$sequence "dieheadshot" {
	"..\anims\dieheadshot"
	ACT_DIE_HEADSHOT 1
	{ event 11 1 }
	{ event 2001 20 }
	fps 30
	X
}
$sequence "diegutshot" {
	"..\anims\diegutshot"
	ACT_DIE_GUTSHOT 1
	{ event 11 0 }
	{ event 2001 29 }
	fps 30
	X
}
$sequence "barnacled1" {
	"..\anims\barnacled1"
	ACT_BARNACLE_HIT 1
	fps 18
}
$sequence "barnacled2" {
	"..\anims\barnacled2"
	ACT_BARNACLE_PULL 1
	fps 30
	loop
}
$sequence "barnacled3" {
	"..\anims\barnacled3"
	ACT_BARNACLE_CHOMP 1
	fps 18
}
$sequence "barnacled4" {
	"..\anims\barnacled4"
	ACT_BARNACLE_CHEW 1
	fps 15
	loop
}
$sequence "repel_jump" {
	"..\anims\repel_jump"
	ACT_HOVER 1
	fps 22
}
$sequence "repel_repel" {
	"..\anims\repel_repel"
	ACT_GLIDE 1
	fps 18
	loop
}
$sequence "repel_shoot" {
	"..\anims\repel_shoot_blend01"
	"..\anims\repel_shoot_blend02"
	ACT_FLY 1
	blend XR -90 45
	{ event 4 1 }
	{ event 5 3 }
	{ event 6 5 }
	{ event 5001 1 }
	{ event 5001 3 }
	{ event 5001 5 }
	fps 18
	loop
}
$sequence "repel_land" {
	"..\anims\repel_land"
	ACT_LAND 1
	fps 20
}
$sequence "crouching_idle" {
	"..\anims\crouching_idle"
	fps 30
	loop
}
$sequence "standing_mp5" {
	"..\anims\standing_mp5_blend01"
	"..\anims\standing_mp5_blend02"
	blend XR -45 45
	{ event 4 0 }
	{ event 5 2 }
	{ event 6 4 }
	{ event 5001 0 "50" }
	{ event 5001 2 "50" }
	{ event 5001 4 "50" }
	fps 50
	loop
}
$sequence "standing_shotgun" {
	"..\anims\standing_shotgun_blend01"
	"..\anims\standing_shotgun_blend02"
	blend XR -45 45
	{ event 4 0 }
	{ event 5001 0 "51" }
	fps 20
}
$sequence "reload_shotgun" {
	"..\anims\reload_shotgun"
	{ event 2 47 }
	fps 40
}
$sequence "dead_on_stomach" {
	"..\anims\dead_on_stomach"
	fps 10
}
$sequence "deadstomach" {
	"..\anims\deadstomach"
	fps 10
}
$sequence "deadside" {
	"..\anims\deadside"
	fps 10
}
$sequence "deadsitting" {
	"..\anims\deadsitting"
	fps 10
}
$sequence "repel_die" {
	"..\anims\repel_die"
	fps 12
}
$sequence "dragholeidle" {
	"..\anims\dragholeidle"
	fps 10
	loop
}
$sequence "draghole" {
	"..\anims\draghole"
	{ event 1000 1 }
	{ event 1004 21 "debris/bustflesh1.wav" }
	fps 10
}
$sequence "bustwall" {
	"..\anims\bustwall"
	{ event 1000 1 }
	fps 20
}
$sequence "hoprail" {
	"..\anims\hoprail"
	fps 22
}
$sequence "converse1" {
	"..\anims\converse1"
	fps 14
	loop
}
$sequence "converse2" {
	"..\anims\converse2"
	fps 14
	loop
}
$sequence "startleleft" {
	"..\anims\startleleft"
	fps 12
}
$sequence "startleright" {
	"..\anims\startleright"
	fps 12
}
$sequence "divecover" {
	"..\anims\divecover"
	fps 22
}
$sequence "defuse" {
	"..\anims\defuse"
	fps 14
	loop
}
$sequence "corner1" {
	"..\anims\corner1"
	fps 14
	loop
}
$sequence "corner2" {
	"..\anims\corner2"
	fps 10
	loop
}
$sequence "stone_toss" {
	"..\anims\stone_toss"
	fps 15
}
$sequence "cliffdie" {
	"..\anims\cliffdie"
	{ event 1001 0 }
	{ event 1000 0 }
	fps 36
	AX AY AZR
}
$sequence "diveaside_idle" {
	"..\anims\diveaside_idle"
	fps 30
	loop
}
$sequence "diveaside" {
	"..\anims\diveaside"
	{ event 4 0 }
	{ event 5 2 }
	{ event 6 4 }
	{ event 4 14 }
	{ event 5 16 }
	{ event 6 18 }
	fps 30
	AX AY AZR
}
$sequence "kneeldive_idle" {
	"..\anims\kneeldive_idle"
	fps 30
	loop
}
$sequence "kneeldive" {
	"..\anims\kneeldive"
	{ event 4 0 }
	{ event 5 2 }
	{ event 6 4 }
	{ event 4 14 }
	{ event 5 16 }
	{ event 6 18 }
	fps 30
	AX AY AZR
}
$sequence "WM_button" {
	"..\anims\wm_button"
	fps 30
	AX AY AZR
}
$sequence "WM_moatjump" {
	"..\anims\wm_moatjump"
	fps 30
	AX AY AZR
}
$sequence "bustwindow" {
	"..\anims\bustwindow"
	{ event 1003 5 "break" }
	fps 24
}
$sequence "dragleft" {
	"..\anims\dragleft"
	fps 28
	loop
	LX
}
$sequence "dragright" {
	"..\anims\dragright"
	fps 28
	loop
	LX
}
$sequence "trackwave" {
	"..\anims\trackwave"
	fps 24
	loop
}
$sequence "trackdive" {
	"..\anims\trackdive"
	{ event 1003 38 "switch" }
	fps 28
}
$sequence "flyback" {
	"..\anims\flyback"
	{ event 1003 29 "zap" }
	{ event 1008 24 "weapons/electro4.wav" }
	fps 25
}
$sequence "impaled" {
	"..\anims\impaled"
	{ event 1000 1 }
	fps 10
}
$sequence "jumptracks" {
	"..\anims\jumptracks"
	fps 30
}
$sequence "pipetoss" {
	"..\anims\pipetoss"
	fps 30
}
$sequence "plunger" {
	"..\anims\plunger"
	fps 22
}

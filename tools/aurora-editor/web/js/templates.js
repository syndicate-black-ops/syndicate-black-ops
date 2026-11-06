// New-file templates. "pfxEd new file" is the default file of XashXT's AuroraEditor (pfxEd), as a study of its
// behavior recorded it; the others are small starting points.

export const TEMPLATES = [
	{
		id: 'pfxed',
		label: 'pfxEd new file',
		note: 'XashXT AuroraEditor\'s new file: an emitter spraying 10 glowing particles a second that rise and fade.',
		name: 'neweffect',
		text: `// New effect (the template of XashXT's AuroraEditor)
"particles" "40"
"maintype" "emitter"

{
  "name" "emitter"
  "spraytype" "particle"
  "sprayrate" "10"
}

{
  "name" "particle"
  "sprite" "sprites/glow01.spr"
  "rendermode" "additive"
  "startsize" "10"
  "endsize" "100"
  "startangle" "0..360"
  "angledelta" "-40..40"
  "gravity" "70"
  "lifetime" "4"
  "startalpha" "1"
  "endalpha" "0"
}
`,
	},
	{
		id: 'blank',
		label: 'Blank',
		note: 'A main type and one sprayed type with nothing set.',
		name: 'neweffect',
		text: `"particles" "100"
"maintype" "source"

{
  "name" "source"
  "spraytype" "particle"
  "sprayrate" "10"
}

{
  "name" "particle"
  "sprite" "sprites/glow01.spr"
  "lifetime" "2"
  "startsize" "8"
}
`,
	},
	{
		id: 'smoke',
		label: 'Smoke column',
		note: 'Alpha-blended puffs that rise, grow, spin and fade (rendermode solid).',
		name: 'smokecolumn',
		text: `// a column of grey smoke
"particles" "120"
"maintype" "source"

{
  "name" "source"
  "spraytype" "smoke"
  "sprayrate" "12"
  "sprayforce" "10..30"
  "spraypitch" "-90..-70"
  "sprayyaw" "0..360"
}

{
  "name" "smoke"
  "sprite" "sprites/fasmoke2.spr"
  "rendermode" "solid"
  "lifetime" "4..6"
  "gravity" "30"
  "drag" "0.5"
  "windyaw" "0"
  "windstrength" "10"
  "startsize" "8"
  "endsize" "48"
  "startalpha" "0.6"
  "endalpha" "0"
  "startred" "0.8"
  "startgreen" "0.8"
  "startblue" "0.8"
  "endred" "0.8"
  "endgreen" "0.8"
  "endblue" "0.8"
  "startframe" "0..100"
  "framerate" "10"
  "startangle" "0..360"
  "angledelta" "-20..20"
}
`,
	},
	{
		id: 'sparks',
		label: 'Spark burst (bounce)',
		note: 'Sparks that fall and bounce on the floor; good as a one-shot (Spawn Use).',
		name: 'sparkburst',
		text: `// sparks that bounce
"particles" "200"
"maintype" "source"

{
  "name" "source"
  "lifetime" "0.5"
  "spraytype" "spark"
  "sprayrate" "120"
  "sprayforce" "80..220"
  "spraypitch" "-80..10"
  "sprayyaw" "0..360"
}

{
  "name" "spark"
  "sprite" "sprites/glow01.spr"
  "rendermode" "additive"
  "lifetime" "1..2"
  "gravity" "-500"
  "collision" "bounce"
  "bounce" "0.4"
  "bouncefriction" "0.2"
  "startsize" "2"
  "endsize" "1"
  "startred" "1"
  "startgreen" "0.8"
  "startblue" "0.4"
  "endred" "1"
  "endgreen" "0.3"
  "endblue" "0"
  "startalpha" "1"
  "endalpha" "0"
}
`,
	},
	{
		id: 'trail',
		label: 'Trail (for a moving target)',
		note: 'Puffs left behind by a moving emitter: try the Scene tab\'s "Moving target" emitter.',
		name: 'trail',
		text: `// a trail: puffs stay where they were sprayed
"particles" "160"
"maintype" "source"

{
  "name" "source"
  "spraytype" "puff"
  "sprayrate" "30"
}

{
  "name" "puff"
  "sprite" "sprites/fasmoke2.spr"
  "rendermode" "solid"
  "lifetime" "3"
  "drag" "2"
  "gravity" "10"
  "startsize" "6"
  "endsize" "24"
  "startalpha" "0.5"
  "endalpha" "0"
  "startframe" "0..100"
  "startangle" "0..360"
}
`,
	},
];

// a new type's starting keys
export const NEW_TYPE_LINES = [
	['sprite', 'sprites/glow01.spr'],
	['rendermode', 'additive'],
	['lifetime', '1'],
	['startsize', '8'],
	['startalpha', '1'],
	['endalpha', '0'],
];

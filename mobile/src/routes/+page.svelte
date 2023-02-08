<script>
	import { connect, JSONCodec } from 'nats.ws';
	import JSONTree from 'svelte-json-tree';
	

	let nc;
	let pos;
	let errors = [];
	let lightStatus;
	let sub;
	let intersectionId;
	let signalGroup;
	let laneManeuvers;

	/*
      This function is called every time the phone "updates" the position. The variable position is an   
      object and has the following shape: 

      position = {
        coords: {
          accuracy: <number-in-meters>,
          altitude: <number-of-meters-above-sea-level>,
          altitudeAccuracy: <number-in-meters>,
          heading: <number-in-degrees-from-true-north>,
          latitude: <number-in-degrees>,
          longitude: <number-in-degrees>,
          speed: <numnber-in-meters-per-second>
        }
      }
    */
	async function setCurrentPosition(position) {
		pos = position;
		let r = await nc.request(
			'light-nearest',
			jc.encode({ lat: position.coords.latitude, lon: position.coords.longitude })
		);
		r = jc.decode(r.data);

		signalGroup = r.signalGroup;
		laneManeuvers = r.laneManeuvers;


		// Start a subscription if new or intersection has changed
		intersectionId = r.intersectionId;
		sub = nc.subscribe(`light-status.${r.intersectionId}`);

		for await (const m of sub) {
			lightStatus = jc.decode(m.data);
		}
	}

	/* This function is called when something went wrong with getting position data. */
	function positionError(error) {
		switch (error.code) {
			case error.PERMISSION_DENIED:
				errors.push('User denied the request for Geolocation.');
				break;

			case error.POSITION_UNAVAILABLE:
				errors.push('Location information is unavailable.');
				break;

			case error.TIMEOUT:
				errors.push('The request to get user location timed out.');
				break;

			case error.UNKNOWN_ERROR:
				errors.push('An unknown error occurred.');
				break;
		}
	}

	// create a codec
	const jc = JSONCodec();

	try {
		connect({
			servers: 'wss://128.46.199.238:4333',
			user: 'diode',
			pass: '9c7TCRO'
		}).then(async (nats) => {
			nc = nats;
			navigator.geolocation.watchPosition(setCurrentPosition, positionError, {
				enableHighAccuracy: true,
				timeout: 15000,
				maximumAge: 0
			});

			console.log('subscription closed');
		});
	} catch (e) {
		console.log(e);
	}
</script>

<h1>Data Diode App</h1>

{#if errors.length > 0}
	<h2 class="error">Errors</h2>
	<ol>
		{#each errors as error}
			<li>{error}</li>
		{/each}
	</ol>
{/if}

{#if !pos}
	<i>Waiting for phone position...</i>
{:else}

<h2>Current Position : <small>Latitude: {pos.coords.latitude}; Longitude: {pos.coords.longitude}</small></h2>

{lightStatus}
{#if lightStatus}
<textarea>

Signal Group: {signalGroup}
Lane Maneuvers allowed : {laneManeuvers}
Light Status: {lightStatus.phases[signalGroup - 1].color}
vehTimeMin : {lightStatus.phases[signalGroup - 1].vehTimeMin}
vehTimeMax : {lightStatus.phases[signalGroup - 1].vehTimeMax}
</textarea>

{:else}
<textarea>

Signal Group: {signalGroup}
Lane Maneuvers allowed : {laneManeuvers}

Waiting for signal data
</textarea>
{/if}

{/if}

<style>
	.error {
		color: red;
	}

	h1 {
			color: #2196f3;
			font-family: 'Comic Sans MS';
			font-size: 3.5em;
			text-align: center;
		}
	h2
	{
			color: grey;
			font-family: 'Calibri';
			font-size: 2em;
			text-align: center;
	}
	textarea {  width: 35%; 
				height: 250px;
				box-sizing: border-box;
				border: 4px solid #d4d4d4;
				display: block;
				margin-left: auto;
    			margin-right: auto;
				font-size: 1.5em;
 				font-family: 'Calibri';
				text-align: center;
				}
</style>

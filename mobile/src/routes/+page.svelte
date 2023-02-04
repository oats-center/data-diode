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

		// Start a subscription if new or intersection has changed
		if (!sub || r.intersectionId !== intersectionId) {
			if (sub) {
				sub.unsubscribe();
			}

			intersectionId = r.intersectionId;
			sub = nc.subscribe(`light-status.${r.intersectionId}`);

			for await (const m of sub) {
				lightStatus = jc.decode(m.data);
			}
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

<h1>Data diode mobile app</h1>

{#if errors.length > 0}
	<h2 class="error">Errors</h2>
	<ol>
		{#each errors as error}
			<li>{error}</li>
		{/each}
	</ol>
{/if}

<h2>Current position</h2>
{#if !pos}
	<i>Waiting for phone position...</i>
{:else}
	<ul>
		<li>Latitude: {pos.coords.latitude}</li>
		<li>Longitude: {pos.coords.longitude}</li>
		<li>Accuracy: {pos.coords.accuracy} m</li>
		<li>Altitude: {pos.coords.altitude} m (accuracy: {pos.coords.altitudeAccuracy} m)</li>
		<li>Heading: {pos.coords.heading} degrees from true north</li>
		<li>Speed: {pos.coords.speed} m/s</li>
	</ul>

	<h2>Intersection: {intersectionId}</h2>

	<h3>Signal group: {signalGroup}</h3>

	{#if lightStatus}
		<JSONTree value={lightStatus.phases[signalGroup - 1]} />

		<h2>Raw data</h2>
		<JSONTree value={lightStatus} />
	{:else}
		<i>Waiting for signal data</i>
	{/if}
{/if}

<style>
	.error {
		color: red;
	}
</style>

<script>
	import 'leaflet/dist/leaflet.css';
	import Leaflet from './Leaflet.svelte';
	import { connect, JSONCodec } from 'nats.ws';
	import JSONTree from 'svelte-json-tree';
	import CircleMarker from './CircleMarker.svelte';

	let nc;
	let pos;
	let errors = [];
	let lightStatus;
	let sub;
	let intersectionId;
	let signalGroup;
	let laneManeuvers;

	async function setCurrentPosition(position) {
		pos = position;
		let r = await nc.request(
			'light-nearest',
			jc.encode({ lat: position.coords.latitude, lon: position.coords.longitude })
		);
		r = jc.decode(r.data);

		signalGroup = r.signalGroup;
		laneManeuvers = r.laneManeuvers;

		// Start a subscription if currently isn't one or the intersection has changed
		if (!sub || r.intersectionId !== intersectionId) {
			if (sub) {
				sub.unsubscribe();
			}

			intersectionId = r.intersectionId;
			sub = nc.subscribe(`light-status.${r.intersectionId}`);
		}

		for await (const m of sub) {
			lightStatus = jc.decode(m.data);
		}

		console.log('Subscription closed.');
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
			servers: 'wss://ibts-compute.ecn.purdue.edu:4333',
			user: 'diode',
			pass: '9c7TCRO'
		}).then(async (nats) => {
			nc = nats;
			navigator.geolocation.watchPosition(setCurrentPosition, positionError, {
				enableHighAccuracy: true,
				timeout: 15000,
				maximumAge: 0
			});
		});
	} catch (e) {
		console.log(e);
	}
</script>

<div>
	<h1 class="text-primary text-4xl">Data Diode App</h1>
</div>

{#if errors.length > 0}
	<h2 class="text-error">Errors</h2>
	<ol>
		{#each errors as error}
			<li>{error}</li>
		{/each}
	</ol>
{/if}

<div class="card bg-base-100 shadow-xl w-full max-w-3xl">
	<div class="card-body">
		{#if !pos}
			<div class="card-title italic">Waiting for phone position...</div>
		{:else}
			<div class="card-title">Current Position</div>
			<Leaflet class="h-56 w-100">
				<CircleMarker lat={pos.coords.latitude} lon={pos.coords.longitude} zoomTo={true} />
			</Leaflet>
			<p class="text-sm">Latitude: {pos.coords.latitude}; Longitude: {pos.coords.longitude}</p>
		{/if}
	</div>
</div>

{#if pos}
	<div class="card bg-base-100 shadow-xl w-full max-w-3xl">
		<div class="card-body">
			<div class="card-title">Signal data</div>
			<p>Group: {signalGroup}</p>
			<p>Allowed Maneuvers: {laneManeuvers}</p>
		</div>
	</div>

	<div class="card bg-base-100 shadow-xl w-full max-w-3xl">
		<div class="card-body">
			{#if lightStatus}
				<div class="card-title">Signal Group Data</div>
				<p>Light Status: {lightStatus.phases[signalGroup - 1].color}</p>
				<p>vehTimeMin : {lightStatus.phases[signalGroup - 1].vehTimeMin}</p>
				<p>vehTimeMax : {lightStatus.phases[signalGroup - 1].vehTimeMax}</p>
			{:else}
				<div class="card-title italic">Waiting for signal data</div>
			{/if}
		</div>
	</div>
{/if}

{#if lightStatus}
	<div class="card bg-base-100 shadow-xl w-full max-w-3xl">
		<div class="card-body">
			<div class="card-title">Raw Signal Data</div>
			<JSONTree value={lightStatus} />
		</div>
	</div>
{/if}

<script lang="ts">
	// FIXME: This whole thing is kinda wonky, but works
	import { getContext, onDestroy } from 'svelte';
	import { browser } from '$app/environment';

	export let lat = false;
	export let lon = false;
	export let zoomTo = false;

	const map = getContext('map')();

	let marker;
	async function setupMarker() {
		marker = (await import('leaflet')).circleMarker([lat, lon], {
			radius: 7,
			color: '#3388ff',
			stroke: false,
			fillOpacity: 0.9
		});

		marker.addTo(map);

		// FIXME: This should be a callable action methinks
		if (zoomTo) {
			map.setView([lat, lon], 14);
		}
	}

	onDestroy(() => {
		marker?.remove();
	});

	let leafletMarker = browser ? setupMarker() : null;
</script>

{#await leafletMarker then _}
	<slot />
{/await}

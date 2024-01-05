<script lang="ts">
	// FIXME: This whole thing is kinda wonky, but works
	import { setContext } from 'svelte';
	import { browser } from '$app/environment';

	import 'leaflet/dist/leaflet.css';

	export let center = [0, 0];
	export let zoom = 0;
	export let zoomControl = true;
	export let dragable = true;
	export let zoomable = true;

	let clazz = '';
	export { clazz as class };

	let m;

	function getMap() {
		return m;
	}
	setContext('map', getMap);

	async function setupLeafletAction() {
		const L = await import('leaflet');
		// Create leaflet environment
		return function createLeaflet(container) {
			m = L.map(container, {
				attributionControl: false,
				zoomControl,
				dragging: dragable,
				doubleClickZoom: zoomable,
				scrollWheelZoom: zoomable,
				boxZoom: zoomable,
				keyboard: zoomable,
				touchZoom: zoomable,
				zoomSnap: 0.5
			});

			if (center && zoom) {
				m.setView(center, zoom);
			}

			L.tileLayer(
				'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}',
				{
					attribution:
						'Tiles &copy; Esri &mdash; Source: Esri, i-cubed, USDA, USGS, AEX, GeoEye, Getmapping, Aerogrid, IGN, IGP, UPR-EGP, and the GIS User Community',
					crossOrigin: true
				}
			).addTo(m);

			L.tileLayer('https://stamen-tiles-{s}.a.ssl.fastly.net/toner-labels/{z}/{x}/{y}{r}.png', {
				attribution:
					'Map tiles by <a href="http://stamen.com">Stamen Design</a>, <a href="http://creativecommons.org/licenses/by/3.0">CC BY 3.0</a> &mdash; Map data &copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
				subdomains: 'abcd',
				minZoom: 0,
				maxZoom: 18
			}).addTo(m);

			return {
				destroy() {
					m?.remove();
					m = undefined;
				}
			};
		};
	}

	// This is pretty stupid!
	let leafletAction = browser ? setupLeafletAction() : null;
</script>

{#await leafletAction}
	<p>Loading ...</p>
{:then createLeaflet}
	{#if createLeaflet}
		<div class={clazz}>
			<div class="map" use:createLeaflet>
				{#if m}
					<slot />
				{/if}
			</div>
		</div>
	{/if}
{/await}

<style>
	.map {
		height: 100%;
	}
</style>

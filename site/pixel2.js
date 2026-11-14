/* Reference-matched Pixel 2, built in Blender by tools/build-pixel2-model.py.
 * The GLB contains the product geometry and PBR materials. Only the display
 * texture, studio lighting and rotation change here. A slow turntable shows
 * the whole product, and pauses whenever the visitor takes control.
 */
import * as THREE from "three";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";
import { RoomEnvironment } from "three/addons/environments/RoomEnvironment.js";

// The README's screenshots, all in the light theme, as on the page beside it
const SHOTS = ["systems", "shelf-n64", "theme", "mp4", "menu"];
const REST = { pitch: 0, yaw: -0.28, roll: 0 };

export async function mount(canvas, { motionAllowed = () => true, screenSource = null, turntable = true, interactive = true, pose = REST } = {}) {
	const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true });
	renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
	renderer.toneMapping = THREE.ACESFilmicToneMapping;
	renderer.toneMappingExposure = 1.15;
	const scene = new THREE.Scene();
	const studio = new RoomEnvironment();
	const strip = new THREE.Mesh(new THREE.PlaneGeometry(2, 10), new THREE.MeshBasicMaterial({ color: 0xffffff, side: THREE.DoubleSide }));
	strip.material.color.multiplyScalar(4);
	strip.position.set(-4, 3, 5); strip.lookAt(0, 0, 0); studio.add(strip);
	// A narrow reflection moves across the metal as the product turns.
	// The studio itself stays still: no flashing lights or travelling spotlight.
	const rimStrip = new THREE.Mesh(new THREE.PlaneGeometry(0.8, 8), new THREE.MeshBasicMaterial({ color: 0xf5ffe9, side: THREE.DoubleSide }));
	rimStrip.material.color.multiplyScalar(2.5);
	rimStrip.position.set(4, 2, -3); rimStrip.lookAt(0, 0, 0); studio.add(rimStrip);
	const pmrem = new THREE.PMREMGenerator(renderer);
	const environment = pmrem.fromScene(studio, 0.04);
	scene.environment = environment.texture;
	pmrem.dispose(); studio.dispose();
	const key = new THREE.DirectionalLight(0xffffff, 2.1);
	key.position.set(-3, 4, 5); scene.add(key);
	const fill = new THREE.DirectionalLight(0xf1f6ff, 0.8);
	fill.position.set(4, 1, 2); scene.add(fill);
	const rim = new THREE.DirectionalLight(0xf5ffe9, 0.85);
	rim.position.set(3, 2, -4); scene.add(rim);
	const camera = new THREE.OrthographicCamera(-1, 1, 1, -1, 0.01, 20);
	camera.position.set(0, 0, 4);

	let model;
	try {
		model = (await new GLTFLoader().loadAsync(new URL("./models/pixel2.glb", import.meta.url).href)).scene;
	} catch (error) {
		environment.dispose(); renderer.dispose(); throw error;
	}
	// Centre the whole physical product, including buttons and shoulders.
	const bounds = new THREE.Box3().setFromObject(model);
	const centre = bounds.getCenter(new THREE.Vector3());
	const height = bounds.getSize(new THREE.Vector3()).y;
	model.scale.multiplyScalar(1 / height);
	model.position.copy(centre).multiplyScalar(-1 / height);
	model.traverse(obj => {
		if (obj.isMesh && obj.material) {
			const materials = Array.isArray(obj.material) ? obj.material : [obj.material];
			materials.forEach(m => { m.envMapIntensity = 1.1; });
		}
	});
	const device = new THREE.Group();
	device.rotation.order = "YXZ";
	device.add(model); scene.add(device);
	device.rotation.set(pose.pitch ?? REST.pitch, pose.yaw ?? REST.yaw, pose.roll ?? REST.roll, "YXZ");
	let visible = false, renderDirty = true;
	function draw() {
		if (screenSource && (!visible || document.hidden)) return;
		renderer.render(scene, camera); renderDirty = false;
	}
	function fit() {
		const w = canvas.clientWidth, h = canvas.clientHeight;
		if (!w || !h) return;
		renderer.setSize(w, h, false);
		const aspect = w / h;
		// Fixed framing keeps the upright product centred during rotation.
		const halfHeight = screenSource ? Math.max(0.56, 0.45 / aspect) : Math.max(0.64, 0.55 / aspect);
		camera.left = -halfHeight * aspect; camera.right = halfHeight * aspect;
		camera.top = halfHeight; camera.bottom = -halfHeight;
		camera.updateProjectionMatrix(); renderDirty = true; draw();
	}

	// Named display mesh lets the physical model remain independent of OS art.
	const display = model.getObjectByName("Screen");
	const screen = screenSource || document.createElement("canvas");
	if (!screenSource) { screen.width = 1024; screen.height = 768; }
	const context = screenSource ? null : screen.getContext("2d");
	const texture = new THREE.CanvasTexture(screen);
	texture.colorSpace = THREE.SRGBColorSpace;
	texture.flipY = false; // glTF UV convention
	texture.anisotropy = Math.min(8, renderer.capabilities.getMaxAnisotropy());
	if (screenSource) {
		texture.minFilter = THREE.NearestFilter;
		texture.magFilter = THREE.NearestFilter;
		texture.generateMipmaps = false;
	}
	if (display) {
		display.material.dispose();
		display.material = new THREE.MeshBasicMaterial({ map: texture, toneMapped: false });
	}
	async function loadImage(name) {
		const image = new Image();
		image.src = new URL(`./img/${name}.png`, import.meta.url).href;
		try { await image.decode(); return image; } catch { return null; }
	}
	const first = screenSource ? null : await loadImage(SHOTS[0]);
	if ((!screenSource && !first) || (screenSource && !display)) {
		environment.dispose(); renderer.dispose(); throw new Error("Device screen could not load");
	}
	const available = first ? [first] : [];
	let shot = 0;
	function showShot() {
		if (!screenSource) {
			context.fillStyle = "#090c0c"; context.fillRect(0, 0, screen.width, screen.height);
			if (available.length) context.drawImage(available[shot % available.length], 0, 0, 1024, 768);
		}
		texture.needsUpdate = true; renderDirty = true; draw();
	}

	let drag = null, frame = 0, lastTime = null;
	let hovered = matchMedia("(hover: hover)").matches && canvas.matches(":hover");
	let ramp = 0, tilt = 0, screenTime = 0;
	const reduce = matchMedia("(prefers-reduced-motion: reduce)");
	function canAnimate() {
		return turntable && visible && !document.hidden && !reduce.matches && motionAllowed()
			&& !hovered && !drag && !canvas.matches(":focus-visible");
	}
	function stopMotion() {
		cancelAnimationFrame(frame); frame = 0;
		lastTime = null; ramp = 0;
	}
	function animate(now) {
		frame = 0;
		if (!canAnimate()) { stopMotion(); return; }
		// Reset time at every restart; a suspended tab must never catch up.
		const dt = lastTime === null ? 0 : Math.max(0, Math.min((now - lastTime) / 1000, 0.05));
		lastTime = now;
		ramp = Math.min(1, ramp + dt / 1.6);
		const speed = ramp * ramp * (3 - 2 * ramp);
		device.rotation.y += (Math.PI * 2 / 42) * speed * dt;
		// The first turn adds a small studio tilt. Manual input ends this step.
		if (tilt < 1) {
			tilt = Math.min(1, tilt + dt / 1.4);
			const ease = tilt * tilt * (3 - 2 * tilt);
			device.rotation.x = REST.pitch + 0.07 * ease;
			device.rotation.z = REST.roll - 0.025 * ease;
		}
		if (available.length > 1) screenTime += dt;
		if (screenTime >= 4) {
			screenTime %= 4; shot++; showShot();
		} else draw();
		frame = requestAnimationFrame(animate);
	}
	function schedule() {
		if (screenSource && renderDirty) draw();
		if (!canAnimate()) { stopMotion(); return; }
		if (!frame) frame = requestAnimationFrame(animate);
	}
	if (interactive) {
		canvas.addEventListener("pointerenter", event => {
			if (event.pointerType === "touch") return;
			hovered = true; schedule();
		});
		canvas.addEventListener("pointerleave", event => {
			if (event.pointerType === "touch") return;
			hovered = false; schedule();
		});
		canvas.addEventListener("pointerdown", event => {
			if (event.button !== 0) return;
			tilt = 1;
			drag = { x: event.clientX, y: event.clientY, pitch: device.rotation.x, yaw: device.rotation.y, id: event.pointerId, type: event.pointerType };
			schedule();
			canvas.setPointerCapture(event.pointerId);
		});
		canvas.addEventListener("pointermove", event => {
			if (!drag || event.pointerId !== drag.id) return;
			device.rotation.y = drag.yaw + (event.clientX - drag.x) * 0.009;
			if (event.pointerType === "mouse") device.rotation.x = Math.max(-0.65, Math.min(0.65, drag.pitch + (event.clientY - drag.y) * 0.006));
			draw();
		});
		const end = event => {
			if (drag?.id !== event.pointerId) return;
			if (drag.type !== "touch") {
				const bounds = canvas.getBoundingClientRect();
				hovered = event.clientX >= bounds.left && event.clientX <= bounds.right
					&& event.clientY >= bounds.top && event.clientY <= bounds.bottom;
			}
			drag = null; schedule();
		};
		canvas.addEventListener("pointerup", end);
		canvas.addEventListener("pointercancel", end);
		canvas.addEventListener("lostpointercapture", end);
		canvas.addEventListener("focus", schedule);
		canvas.addEventListener("blur", schedule);
		canvas.addEventListener("keydown", event => {
			schedule();
			if (event.key === "ArrowLeft") device.rotation.y -= 0.12;
			else if (event.key === "ArrowRight") device.rotation.y += 0.12;
			else return;
			tilt = 1; event.preventDefault(); draw();
		});
	}
	new IntersectionObserver(entries => {
		visible = entries[entries.length - 1].isIntersecting; schedule();
	}).observe(canvas);
	document.addEventListener("visibilitychange", schedule);
	reduce.addEventListener("change", schedule);
	document.addEventListener("zyperos-motion", schedule);
	new ResizeObserver(fit).observe(canvas);
	fit(); showShot();
	// The first screen is enough to reveal the product. Warm the small tour
	// afterwards, without delaying the model or downloading the old full gallery.
	if (!screenSource) Promise.all(SHOTS.slice(1).map(loadImage)).then(images => {
		available.push(...images.filter(Boolean)); schedule();
	});
	// A live display owns its frame timing. Static game models never need an
	// independent animation loop; the caller updates after drawing its canvas.
	return Object.freeze({ updateScreen: showShot });
}

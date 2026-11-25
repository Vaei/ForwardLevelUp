# ForwardLevelUp

Clean MSAA and proper ambient occlusion for Unreal Engine 5.8's mobile forward renderer, on PC and mobile hardware. Engine patches for a source build; everything is off until you turn it on.

> [!IMPORTANT]
> Level up Unreal Engine's Mobile Forward Rendering Path to Modern Standards: MSAA, SSAO, DFAO, SSR without TAA, analytic capsule AO.

## Comparisons & Gameplay Videos

[Click Me - The site is better than this readme](https://vaei.github.io/ForwardLevelUp/)

[![Before and after gameplay](https://img.youtube.com/vi/Wv0jJrZ6ruw/maxresdefault.jpg)](https://youtu.be/Wv0jJrZ6ruw)

## What you get:

- Smoother MSAA edges, without shadow and AO halos around them.
- Ambient occlusion that works in forward: screen-space, distance-field, contact shadows under characters and any movable object, and street-scale occlusion from buildings, in a window that follows the camera.
- Coloured bounce light off the buildings around you.
- Deferred decals that change the surface every light shades, normals and roughness included, correct at every MSAA sample.
- Soft particles without halos around foreground edges.
- Screen-space reflections under MSAA without crashing, and reflections of moving objects that stay attached to them.
- Virtual shadow maps without Nanite or TAA: sharp, detailed sun and local-light shadows at every distance, filtered without temporal noise.
- Diffuse GI from a grid of probes around the camera: interiors and covered streets stop glowing with sky light, and sun, sky, neon and local lights bounce.
- Optionally, world normals for post-process materials.

## Get it working

You need a UE 5.8 source build, and a project using the mobile renderer (`bPreferFeatureLevelES31` on PC, or a mobile platform).

1. Apply the patches to the engine, in order:

   ```
   cd "<Engine root>"
   git apply <this repo>/Patches/1-wide-resolve.patch
   git apply <this repo>/Patches/2-df-shadow-msaa-fix.patch
   git apply <this repo>/Patches/3-edge-aware-fetch-and-ao.patch
   git apply <this repo>/Patches/4-skip-redundant-resolve.patch
   git apply <this repo>/Patches/5-forward-dfao.patch
   git apply <this repo>/Patches/6-mobile-capsule-ao-and-ssr-fix.patch
   git apply <this repo>/Patches/7-forward-normal-target.patch
   git apply <this repo>/Patches/8-mobile-height-field-ao.patch
   git apply <this repo>/Patches/9-tonemap-subpass-resolve.patch
   git apply <this repo>/Patches/10-per-sample-depth-fade.patch
   git apply <this repo>/Patches/11-mobile-primitive-ao.patch
   git apply <this repo>/Patches/12-height-field-clipmap.patch
   git apply <this repo>/Patches/13-height-field-gi.patch
   git apply <this repo>/Patches/14-clustered-decals.patch
   git apply <this repo>/Patches/15-ssr-velocity.patch
   git apply <this repo>/Patches/16-virtual-shadow-maps.patch
   git apply <this repo>/Patches/17-probe-gi.patch
   ```

2. Build the editor. The first launch recompiles every material.
3. Add this to `Config/DefaultEngine.ini` under `[/Script/Engine.RendererSettings]`:

   ```ini
   ; Mobile forward. 5.8 defaults to mobile deferred, which cannot use MSAA.
   r.Mobile.ShadingPath=0
   r.Mobile.AntiAliasing=3
   r.MSAACount=4
   r.WideCustomResolve=2
   r.Mobile.MSAA.EdgeAwareFetch=1
   r.Mobile.MSAA.SkipRedundantResolve=1
   r.Mobile.AmbientOcclusion=1
   r.DefaultFeature.AmbientOcclusion=True
   ```

4. Restart the editor. Edges are smooth, and thin railings and poles stop shimmering.

### Optional extras

Each needs the setup above.

- **Distance-field AO:** `r.Mobile.DistanceFieldAO=1`. Needs `r.GenerateMeshDistanceFields=True` and a Movable sky light with Cast Shadows.
- **Contact shadows under characters:** `r.Mobile.EnableCapsuleShadows=1` and `r.CapsuleIndirectSkyLightFromAbove=1`, with distance-field AO on. On each character, tick Cast Capsule Indirect Shadow on the skeletal mesh component and set a Shadow Physics Asset on the skeletal mesh.
- **Screen-space reflections:** `r.Mobile.ScreenSpaceReflections=1`.
- **Occlusion from buildings:** `r.Mobile.HeightFieldAO=1`. Copy `Examples/HeightFieldAOCapture` into your game module, add `"Renderer"`, `"RenderCore"` and `"RHI"` to its dependencies, then place the actor and size its box over the level. For a level bigger than one capture, tick **Follow View**: a window of tiles follows the camera and only the tiles it moves onto are captured, a few per frame.
- **Bounce light from buildings:** `r.Mobile.HeightFieldGI=1`, with the capture actor above.
- **Contact shadows under anything movable, no distance fields:** `r.Mobile.PrimitiveAO=1`. Characters use their shadow capsules as above; on movable static meshes tick **Cast Distance Field Indirect Shadow** (no distance field is needed, an ellipsoid is fitted to the bounds).
- **Decals in the base pass:** `r.Mobile.ClusteredDecals=1`. Decals that write base colour, normal or roughness are applied before lighting, lit by every light. Emissive, modulate and alpha composite decals keep the stock path.
- **Soft particles without halos:** `r.Mobile.MSAA.PerSampleDepthFade=1`. PC only.
- **Tonemap subpass on Vulkan devices:** with `r.Mobile.TonemapSubpass=1`, set `r.Mobile.TonemapSubpass.Resolve=2` to tonemap each sample before averaging, so bright edges stop stair-stepping. The PC resolve already does the equivalent.
- **World normals in post-process materials:** `r.Mobile.Forward.NormalTarget=1`. SceneTexture WorldNormal then works in post-process materials.
- **Reflections that follow moving objects:** `r.Mobile.ScreenSpaceReflections.Velocity=1`, with screen-space reflections on. Renders velocities without TAA, MSAA included, so a reflected car stays under the car. Not on OpenGL.
- **Virtual shadow maps:** `r.Mobile.VirtualShadowMaps=1`, `r.Shadow.Virtual.Enable=1` and `r.Mobile.SupportGPUScene=1`. Replaces cascaded and local-light shadow maps for movable lights; no Nanite needed. On Windows it needs DirectX 12 (`DefaultGraphicsRHI_DX12`, or `-d3d12`). Not on OpenGL.
- **Probe GI:** `r.Mobile.ProbeGI=1`, with `r.GenerateMeshDistanceFields=True`. Probes around the camera trace the global distance field and replace the sky light's diffuse inside their grid. Ray hits on screen take last frame's lit colour, which needs `r.Mobile.ScreenSpaceReflections=1`; hits off screen use the sun, sky and up to 64 nearby point, spot and rect lights.

## Troubleshoot

- **No anti-aliasing at all.** Check `r.Mobile.ShadingPath=0`. Under mobile deferred every MSAA setting is ignored.
- **AA settings from the console do nothing in the editor.** The mobile preview device profile overrides them. Set them in the ini and restart.
- **Ini settings ignored.** A `# comment` after a value becomes part of the value. Put comments on their own `;` line.
- **Point and spot lights went dark.** Forward needs `r.Mobile.Forward.EnableLocalLights=1`.
- **No AO.** `r.Mobile.AmbientOcclusion=1` and `r.DefaultFeature.AmbientOcclusion=True` (or a post-process volume with AO intensity above 0). Distance-field, capsule and height-field AO all draw into the SSAO texture, so they need it too.
- **Distance-field AO missing or too dark.** It needs mesh distance fields and a Movable sky light with Cast Shadows and no static lighting. Its strength comes from the sky light: lower Occlusion Max Distance or raise Min Occlusion.
- **No contact shadow under characters.** Check both character settings above. If the sky's brightest direction is near the horizon, `r.CapsuleIndirectSkyLightFromAbove=1` is what makes it show.
- **Height-field AO does nothing.** `ProfileGPU` shows `HeightFieldAO` when it runs. Check the capture actor's box covers the camera, and that SSAO is on.
- **Height-field AO darkens walls under roof overhangs.** Raise `r.Mobile.HeightFieldAO.NormalBias` past the overhang depth.
- **Post-process material fails with "GBuffer scene textures not available with forward shading".** Set `r.Mobile.Forward.NormalTarget=1` and restart. Only WorldNormal is supported. A warning for PCD3D_SM5 remains, which is harmless when you only preview mobile.
- **Height-field AO fades along one edge after the camera moves (Follow View).** Those tiles are still being captured. Raise Tiles Per Tick.
- **Bounce light the wrong colour.** The capture must render unlit; its colour is the albedo that bounces.
- **Primitive AO does nothing.** Needs SSAO on and the flags above. Instanced static meshes are skipped.
- **Clustered decal settings ignored.** `r.Mobile.ClusteredDecals`, `r.Mobile.HeightFieldGI` and `r.Mobile.MSAA.PerSampleDepthFade` are read only: set them in the ini and restart. They recompile materials.
- **A decal looks frozen.** Decals are baked when they change. For materials animated by time set `r.Mobile.ClusteredDecals.RebakeEveryFrame=1`.
- **Desktop Vulkan at ES3.1 with MSAA crashes in `vkCmdBeginRenderingKHR` (seen on NVIDIA).** Stock 5.8, with every patch setting at its default too. Set `r.Vulkan.AllowDynamicRendering=0` (read only, in the ini or with `-dpcvars`). The stock tonemap subpass on desktop Vulkan also asserts on a missing `FMobileMultiViewVertexShaderVS`; patch 9 only matters on devices with an on-chip MSAA resolve (Adreno).
- **Unlit or translucent materials do not get the edge fixes or AO.** Only opaque and masked lit materials do.
- **Velocity, virtual shadow map and probe GI settings ignored.** `r.Mobile.ScreenSpaceReflections.Velocity`, `r.Mobile.VirtualShadowMaps`, `r.Mobile.VirtualShadowMaps.FilterTaps` and `r.Mobile.ProbeGI` are read only: set them in the ini and restart. They recompile materials.
- **Fatal error "r.Mobile.VirtualShadowMaps compiles shadow shaders with DXC, which DirectX 11 cannot run".** Switch the project to DirectX 12, or turn `r.Mobile.VirtualShadowMaps` off.
- **No virtual shadows.** `ProfileGPU` shows `VirtualShadowMapMarkPages` when they run. Check `r.Shadow.Virtual.Enable=1`, `r.Mobile.SupportGPUScene=1`, and that the light is Movable.
- **Surfaces shadow themselves in stripes.** Raise `r.Mobile.VirtualShadowMaps.NormalBias`.
- **First launch with virtual shadow maps takes minutes.** Their shaders compile once for the mobile shader platform, then come from the DDC.
- **Probe GI does nothing.** `ProfileGPU` shows `MobileProbeGI` when it runs. It needs mesh distance fields; `r.Mobile.ProbeGI.Intensity` above 0.
- **Interiors went dark with probe GI.** Expected: the sky light no longer leaks through walls and roofs. Light reaches the probes through openings and from lights inside.
- **Blotches of light on walls with probe GI.** Raise `r.Mobile.ProbeGI.RaysPerProbe` or `r.Mobile.ProbeGI.Hysteresis`, or lower `r.Mobile.ProbeGI.Spacing`.
- **Light leaks through a thin wall with probe GI.** Walls thinner than a global distance field voxel do not stop probe rays. Thicken the wall or its distance field resolution.

## Tune it

| Setting | Default | What it does |
|---|---|---|
| `r.MSAACount` | 4 | 2, 4 or 8 samples. |
| `r.WideCustomResolve` | 0 | Resolve filter width: 0 stock, 1 to 3 wider and smoother. 2 balances sharpness and smoothness. |
| `r.Mobile.MSAA.EdgeDepthThreshold` | 0.02 | When an edge pixel counts as a different surface. Raise it if floors at grazing angles flicker. |
| `r.Mobile.DistanceFieldAO` | 0 | About 0.25 ms. Strength is set on the sky light. |
| `r.Mobile.HeightFieldAO.MaxDistance` | 3000 | How far away a building can occlude, in world units. |
| `r.Mobile.HeightFieldAO.MinDistance` | 0 | Raise to the sky light's Occlusion Max Distance so it does not double up with distance-field AO. |
| `r.Mobile.HeightFieldAO.Intensity` | 1 | 0 to 1. |
| `r.Mobile.HeightFieldAO.NormalBias` | 150 | Raise if walls darken under overhangs. |
| `r.Mobile.HeightFieldAO.Directions` / `.Steps` | 8 / 8 | Quality against cost. About 0.18 ms at 1901x1269 at the defaults. |
| `r.Mobile.HeightFieldGI.Intensity` | 1 | Strength of the bounce. About 0.02 ms at the defaults, at half resolution. |
| `r.Mobile.HeightFieldGI.Directions` / `.Steps` | 6 / 6 | Quality against cost. |
| `r.Mobile.PrimitiveAO.MaxDistance` | 100 | How far past a shape it occludes. About 0.045 ms for 12 casters. |
| `r.Mobile.PrimitiveAO.EllipsoidScale` | 1.2 | Ellipsoid size against a static mesh's bounds. Lower it if vehicle interiors go dark. |
| `r.Mobile.ClusteredDecals.AtlasSize` / `.TileSize` | 2048 / 256 | The atlas holds (AtlasSize / TileSize) squared decals. |
| `r.Mobile.ClusteredDecals.MaxPerTile` | 8 | Decals one 64 px screen tile applies. |
| `r.Mobile.ScreenSpaceReflections.Steps` | 0 | Reflection ray march steps. 0 keeps 8, or 12 with velocity. Read only. |
| `r.Mobile.VirtualShadowMaps.FilterRadius` | 1.5 | Shadow edge softness, in screen pixels at the receiver. 0 is hard. |
| `r.Mobile.VirtualShadowMaps.FilterTaps` | 5 | Lookups per light per pixel: 1, 5 or 9. Read only. |
| `r.Mobile.VirtualShadowMaps.NormalBias` | 2 | How far lookups step off the surface, in screen pixels. Raise it if surfaces shadow themselves. |
| `r.Mobile.ProbeGI.Spacing` | 200 | World distance between probes. About 0.24 ms for the whole update at the defaults. |
| `r.Mobile.ProbeGI.GridSizeXY` / `.GridSizeZ` | 32 / 16 | Probes across and up. The grid covers this times the spacing, centred on the camera. |
| `r.Mobile.ProbeGI.RaysPerProbe` | 64 | Rays per probe update, 32 to 256. More is smoother and costs more. |
| `r.Mobile.ProbeGI.UpdateInterval` | 4 | Each probe updates every this many frames. |
| `r.Mobile.ProbeGI.Hysteresis` | 0.95 | How much of its last value a probe keeps per update. Lower follows moving lights faster. |
| `r.Mobile.ProbeGI.Intensity` | 1 | Scale on the probe light. 0 restores the sky light. |
| `r.Mobile.ProbeGI.Albedo` | 0.3 | Colour assumed for off-screen ray hits. |
| `r.Mobile.ProbeGI.MaxTraceDistance` | 10000 | How far a probe ray goes before it counts as sky. |
| `r.Mobile.ProbeGI.ScreenRadiance` | 1 | Whether on-screen hits take last frame's lit colour. |
| `r.Mobile.ProbeGI.LocalLights` / `.LocalLightShadows` | 64 / 1 | Local lights that light off-screen hits, and whether they are shadowed through the distance field. |

On materials, no patch needed: tick **Normal Curvature To Roughness** to stop specular shimmer MSAA cannot fix, and **Use Alpha To Coverage** on masked foliage and fences so their cut-out edges get MSAA too.

Each patch and every setting is documented on the [site](https://vaei.github.io/ForwardLevelUp/).

## Resources

* [Mobile Forward Rendering Guide](https://blog.daftsoftware.com/unreal-perf-maxing/)
  * Includes Convenient Starter Project
* [MobMaterials Plugin](https://github.com/Vaei/MobMaterials)
  * Landscape, surface and foliage masters for the same renderer, which is what these lights land on
* [MobFort Plugin](https://github.com/Vaei/MobFort)
  * Stylized unlit character masters for the same renderer. Characters author their own light rather than taking it from these
* [MobWater Plugin](https://github.com/Vaei/MobWater)
  * Ponds, lakes, rivers and ocean on the same renderer, that characters wade and swim in
* [MobLights Plugin](https://github.com/Vaei/MobLights)
  * Local lights for the mobile forward rendering path, and fog to put them in
* [MobSky Plugin](https://github.com/Vaei/MobSky)
  * Sky for the mobile forward rendering path. Volumetric clouds you fly through, an atmosphere that thins to space, and a space backdrop of stars, a galaxy and nebulae
* [Forward Render Helper Plugin](https://github.com/Vaei/ForwardRender)
  * Replaces Hotpatch Module in Perf Maxing guide above
# Reflex 2 / Latewarp Notes

These notes are recovered from `Reflex2Demo2/bin/StreamlineSample.pdb`,
`StreamlineSample.exe`, `nvngx.dll`, and `nvngx_latewarp.dll`.

The demo does not include C++ source or headers, but the PDB confirms that it was
built with `STREAMLINE_FEATURE_LATEWARP` and used:

- `nvsdk_ngx_helpers_latewarp.h`
- `NVSDK_NGX_D3D12_Latewarp_Eval_Params`
- `NVSDK_NGX_Latewarp_Create_Params`
- `NGXWrapper::EvaluateLatewarp`

`sl::kFeatureLatewarp` is `6` in the sample, but the NGX feature enum used by
`NVSDK_NGX_D3D12_CreateFeature` maps Latewarp to
`NVSDK_NGX_Feature_Reserved15`, value `15`.

## Required Runtime Files

Stage these beside the existing Streamline runtime DLLs:

- `nvngx.dll`
- `nvngx_latewarp.dll`

`Reflex2Demo2` also contains `nvngx_dlss.dll`, but this repo already packages a
DLSS runtime. Do not overwrite that DLL without separately checking versions and
compatibility.

## Important Parameter Keys

Create:

- `Latewarp.Output.Width`
- `Latewarp.Output.Height`

Evaluate:

- `Output`
- `Latewarp.Backbuffer`
- `Latewarp.HudlessColor`
- `Latewarp.UIColorAlpha`
- `Depth`
- `MotionVectors`
- `Latewarp.NoWarpMask`
- `Latewarp.Output.Subrect.Base.X`
- `Latewarp.Output.Subrect.Base.Y`
- `Latewarp.Output.Subrect.Width`
- `Latewarp.Output.Subrect.Height`
- `Latewarp.Backbuffer.Subrect.Base.X`
- `Latewarp.Backbuffer.Subrect.Base.Y`
- `Latewarp.Backbuffer.Subrect.Width`
- `Latewarp.Backbuffer.Subrect.Height`
- `Latewarp.HudlessColor.Subrect.Base.X`
- `Latewarp.HudlessColor.Subrect.Base.Y`
- `Latewarp.HudlessColor.Subrect.Width`
- `Latewarp.HudlessColor.Subrect.Height`
- `Latewarp.UIColorAlpha.Subrect.Base.X`
- `Latewarp.UIColorAlpha.Subrect.Base.Y`
- `Latewarp.UIColorAlpha.Subrect.Width`
- `Latewarp.UIColorAlpha.Subrect.Height`
- `Latewarp.Depth.Subrect.Base.X`
- `Latewarp.Depth.Subrect.Base.Y`
- `Latewarp.Depth.Subrect.Width`
- `Latewarp.Depth.Subrect.Height`
- `Latewarp.MV.Subrect.Base.X`
- `Latewarp.MV.Subrect.Base.Y`
- `Latewarp.MV.Subrect.Width`
- `Latewarp.MV.Subrect.Height`
- `Latewarp.NoWarpMask.Subrect.Base.X`
- `Latewarp.NoWarpMask.Subrect.Base.Y`
- `Latewarp.NoWarpMask.Subrect.Width`
- `Latewarp.NoWarpMask.Subrect.Height`
- `Latewarp.FrameID`
- `Latewarp.IsRenderedFrame`
- `Latewarp.DepthInverted`
- `Latewarp.UsePremultiplyUIAlpha`
- `Latewarp.EvalFlags`
- `Latewarp.Reserved1`
- `Jitter.Offset.X`
- `Jitter.Offset.Y`
- `Latewarp.WorldToViewMatrix`
- `Latewarp.ViewToClipMatrix`
- `Latewarp.PrevRenderedWorldToViewMatrix`
- `Latewarp.PrevRenderedViewToClipMatrix`

## FO4 Integration Shape

The existing DLSS-G capture path already collects most Latewarp inputs:

- HUD-less color: `dlssgHUDLessD3D12`
- motion vectors: `dlssgMotionVectorD3D12`
- depth: `dlssgDepthD3D12`
- UI color/alpha: `dlssgUIColorAlphaD3D12`
- frame ID: current game frame/frame token index

The next wiring point should be the D3D12 present path after the D3D11-to-D3D12
copies are synchronized and before present. Use a separate UAV-capable output
texture first, then copy to the swapchain buffer after Latewarp succeeds. Avoid
evaluating straight into the swapchain backbuffer until the resource flags and
states are proven safe.

The current test hook follows that shape behind `bReflex2LatewarpEnabled`.
Enable it in `Data/MCM/Settings/Upscaling.ini` or through MCM when testing.

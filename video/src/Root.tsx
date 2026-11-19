import React from "react";
import { CalculateMetadataFunction, Composition, staticFile, Still } from "remotion";
import { Tour, TourProps } from "./Tour";
import { Poster, PosterProps } from "./Poster";
import { Clip, CLIPS, FPS, TOTAL } from "./timeline";
import { H as REDDIT_H, Reddit, RedditProps, W as REDDIT_W } from "./reddit/Reddit";
import { REDDIT } from "./reddit/timeline";
import { Release106, Release106Props } from "./v106/Release106";
import { PAIR, R106 } from "./v106/timeline";

/** The footage index (video/footage/takes.py) and the device's screen rect (video/device) */
const load = async () => {
  const [footage, device] = await Promise.all([
    fetch(staticFile("footage/index.json")).then((r) => r.json()),
    fetch(staticFile("device/pixel2-front.json")).then((r) => r.json()),
  ]);
  return { footage, device };
};

/** Every clip inside its take: a scene longer than its footage would show black */
const check = (footage: Record<string, { ms: number }>, clips: Clip[] = CLIPS) => {
  for (const c of clips) {
    if (c.kind !== "take") continue;
    const need = c.fromMs + (c.len / FPS) * 1000 * c.rate;
    const have = footage[c.take]?.ms ?? 0;
    if (need > have + 20) throw new Error(`${c.scene}: ${c.take} needs ${Math.round(need)} ms, has ${have}`);
  }
};

const tourMeta: CalculateMetadataFunction<TourProps> = async () => {
  const props = await load();
  check(props.footage);
  return { props };
};
const redditMeta: CalculateMetadataFunction<RedditProps> = async () => {
  const props = await load();
  check(props.footage, REDDIT.clips);
  return { props };
};
/** 1.0.6's: the Brick beside the Pixel 2, and every clip of both handhelds inside its take */
const release106Meta: CalculateMetadataFunction<Release106Props> = async () => {
  const props = await load();
  const brick = await fetch(staticFile("device/brick-front.json")).then((r) => r.json());
  check(props.footage, [...R106.clips, ...PAIR.pixel2.clips, ...PAIR.brick.clips]);
  return { props: { ...props, brick } };
};
const posterMeta: CalculateMetadataFunction<PosterProps> = async ({ props }) => ({ props: { ...props, ...(await load()) } });

const empty = { footage: {}, device: { image: "", width: 2000, height: 2400, screen: { x: 0, y: 0, w: 1, h: 1 } } };

export const Root: React.FC = () => (
  <>
    <Composition id="ZyperOSTour" component={Tour} durationInFrames={TOTAL} fps={FPS} width={1920} height={1080}
      defaultProps={empty as TourProps} calculateMetadata={tourMeta} />
    {/* The site's poster, and the README's, which carries a play button */}
    <Still id="Poster" component={Poster} width={1920} height={1080}
      defaultProps={{ ...empty, play: false } as PosterProps} calculateMetadata={posterMeta} />
    <Still id="PosterPlay" component={Poster} width={1920} height={1080}
      defaultProps={{ ...empty, play: true } as PosterProps} calculateMetadata={posterMeta} />
    {/* 1.0.5's new features alone, 4:5 for a phone's feed (src/reddit) */}
    <Composition id="Reddit105" component={Reddit} durationInFrames={REDDIT.total} fps={FPS} width={REDDIT_W} height={REDDIT_H}
      defaultProps={empty as RedditProps} calculateMetadata={redditMeta} />
    {/* 1.0.6's new features and the TrimUI Brick, for the ZyperOS Discord (src/v106) */}
    <Composition id="Release106" component={Release106} durationInFrames={R106.total} fps={FPS} width={1920} height={1080}
      defaultProps={{ ...empty, brick: empty.device } as Release106Props} calculateMetadata={release106Meta} />
  </>
);

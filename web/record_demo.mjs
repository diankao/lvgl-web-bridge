/* record_demo.mjs — Playwright 驱动网页模拟器并录制视频
 * 坐标是 UI 相关的(本例为 OV-Watch 240x280), 换自己的 UI 时改成你的坐标即可
 */
import { chromium } from 'playwright';
import fs from 'fs';

fs.mkdirSync('videos', { recursive: true });

const browser = await chromium.launch({ channel: 'msedge' });
const ctx = await browser.newContext({
  viewport: { width: 560, height: 720 },
  recordVideo: { dir: 'videos', size: { width: 560, height: 720 } },
});
const page = await ctx.newPage();
await page.goto('http://127.0.0.1:8090/');
await page.waitForTimeout(2000);

const box = await page.locator('#screen').boundingBox();
const { fw, fh } = await page.evaluate(() =>
  ({ fw: document.getElementById('screen').width, fh: document.getElementById('screen').height }));
const to = (x, y) => [box.x + x / fw * box.width, box.y + y / fh * box.height];

async function tap(x, y) {
  const [px, py] = to(x, y);
  await page.mouse.click(px, py);
}
async function swipe(x1, y1, x2, y2, ms = 450) {
  const [ax, ay] = to(x1, y1), [bx, by] = to(x2, y2);
  await page.mouse.move(ax, ay);
  await page.mouse.down();
  for (let i = 1; i <= 10; i++) {
    await page.mouse.move(ax + (bx - ax) * i / 10, ay + (by - ay) * i / 10);
    await page.waitForTimeout(ms / 10);
  }
  await page.mouse.up();
}

/* 演示序列: 主页 → 右滑进菜单 → 滚动菜单 → 点一个应用 → 返回 */
await page.waitForTimeout(1000);            // 主页停留
await swipe(30, 150, 210, 150);             // 右滑 → 菜单
await page.waitForTimeout(1800);
await swipe(120, 220, 120, 60);             // 菜单上滑
await page.waitForTimeout(1200);
await swipe(120, 60, 120, 220);             // 菜单下滑
await page.waitForTimeout(1200);
await tap(120, 140);                        // 点菜单项
await page.waitForTimeout(2500);
await swipe(20, 150, 220, 150);             // 右滑返回
await page.waitForTimeout(2000);

const video = page.video();
await ctx.close();
await video.saveAs('videos/demo.webm');
console.log('视频已保存: videos/demo.webm');
await browser.close();

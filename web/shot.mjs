/* shot.mjs — 打开网页模拟器截一张图（用于验证画面） */
import { chromium } from 'playwright';

const browser = await chromium.launch({ channel: 'msedge' });
const page = await browser.newPage({ viewport: { width: 560, height: 720 } });
await page.goto('http://127.0.0.1:8090/');
await page.waitForTimeout(2500);
await page.screenshot({ path: 'shot.png' });
console.log('saved shot.png');
await browser.close();

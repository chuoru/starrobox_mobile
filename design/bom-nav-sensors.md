# BOM: navigation and safety sensors (magnetic tracking, RFID, anti-fall)

Design: `design/magnetic-rfid-cliff.md`. Estimated total about USD 60 (about JPY 9,000), excluding the existing chassis, Nexus board and AtomS3. Prices are estimates and were not checked against shop pages.

Link types: **page** = a product page found by web search on 2026-10-25; **search** = a shop search URL built by hand and not opened, so open it and pick a listing. Check price and stock before ordering.

| # | Item | Purpose | Qty | Est. unit (USD) | Procurement (Japan) |
|---|---|---|---|---|---|
| 1 | Omnipolar Hall switch, TI DRV5032 family (the FB variant is marked obsolete on DigiKey; use an active one such as AJ, TO-92, about 9.5 mT trip) | Magnetic tracking: detect the tape under the robot | 16 (8 front, 8 rear) | 0.6-1 | page: [DigiKey JP, DRV5032AJ](https://www.digikey.jp/en/products/detail/texas-instruments/DRV5032AJLPG/12165247); [TI datasheet](https://www.ti.com/lit/gpn/DRV5032); search: [Mouser JP](https://www.mouser.jp/c/?q=DRV5032) |
| 2 | Hall array PCB, 10 mm pitch | Magnetic tracking: holds 8 Hall sensors at 10 mm pitch | 2 | 5 | custom, JLCPCB or PCBWay |
| 3 | MCP23017 DIP-28 I2C expander (or PCF8574 breakout; PCF8574 is mostly SSOP-20) | Magnetic tracking: reads the 8 Hall outputs over I2C | 2 | 1-2 | search: [Akizuki](https://akizukidenshi.com/catalog/goods/search.aspx?search=x&keyword=MCP23017), [DigiKey JP](https://www.digikey.jp/en/products?keywords=MCP23017-E%2FSP) |
| 4 | Magnetic guide tape, adhesive (Misumi: Accurate Systems AGV guide tape, 25 mm class; Makome: 50 mm x 1 mm) | Magnetic tracking: the route the robot follows | 5 m | check | page: [Misumi AGV guide tape](https://jp.misumi-ec.com/vona2/detail/223012345305/), [Misumi tape category](https://jp.misumi-ec.com/vona2/s_cate/%E7%A3%81%E6%B0%97%20%E3%83%86%E3%83%BC%E3%83%97), [Makome](https://pr.mono.ipros.com/macome/product/detail/2000321633/) |
| 5 | M5Stack Unit RFID2 (U031-B, WS1850S, I2C 0x28) | Seat localization: reads the tag at each seat | 1 | 5-9 | search: [Switch Science](https://www.switch-science.com/search?q=RFID2); page: [DigiKey JP](https://www.digikey.jp/en/products/detail/m5stack-technology-co-ltd/U031-B/15965508) (a 30-week lead time was listed) |
| 6 | NTAG213 NFC sticker, 25 mm round | Seat localization: one tag per seat stop and home | 20 | 0.3-1.4 | search: [Amazon JP](https://www.amazon.co.jp/s?k=NTAG213+%E3%82%B7%E3%83%BC%E3%83%AB+25mm), [Switch Science](https://www.switch-science.com/search?q=NTAG213) |
| 7 | 13.56 MHz ferrite shield sheet (Wurth WE-FSFS or 3M FFDM) | Seat localization: keeps the metal chassis from detuning the reader | 1 | 3 | page: [Wurth WE-FSFS at RS](https://uk.rs-online.com/web/p/shielding-sheets/8235358P); search: [Mouser JP](https://www.mouser.jp/c/?q=WE-FSFS) |
| 8 | TCRT5000 IR reflective module, digital output (detection range varies by listing, 1-8 mm typical, so mount low) | Anti-fall: see the table surface at each corner | 4 | 1-2 | search: [Amazon JP](https://www.amazon.co.jp/s?k=TCRT5000+%E3%83%A2%E3%82%B8%E3%83%A5%E3%83%BC%E3%83%AB), [Akizuki](https://akizukidenshi.com/catalog/goods/search.aspx?search=x&keyword=TCRT5000), [Marutsu](https://www.marutsu.co.jp/pc/s/TCRT5000/) |
| 9 | 74HC21 DIP-14 dual 4-input AND (use one gate) plus 100 nF capacitor; three 74HC08 also work | Anti-fall: combines 4 sensors into one fail-safe line to D6 | 1 | 0.5 | search: [Akizuki](https://akizukidenshi.com/catalog/goods/search.aspx?search=x&keyword=74HC21), [DigiKey JP](https://www.digikey.jp/en/products?keywords=SN74HC21N) |
| 10 | Whisker / micro switch (backup edge detection) | Anti-fall: mechanical backup if the IR sensors miss the edge | 2 | 1 | search: [Akizuki](https://akizukidenshi.com/catalog/goods/search.aspx?search=x&keyword=%E3%83%9E%E3%82%A4%E3%82%AF%E3%83%AD%E3%82%B9%E3%82%A4%E3%83%83%E3%83%81) |

## Known gaps
- The searches found no Japanese shop pages for Akizuki, Switch Science, Marutsu or Amazon JP; those rows are search links.
- The tape width must match the Hall array (8 sensors at 10 mm cover 70 mm, which fits both 25 mm and 50 mm tape). The Hall part must be re-checked against the tape's field at 3-5 mm.
- Ferrite sheet sold in Japan was not found; Wurth parts at RS are overseas.

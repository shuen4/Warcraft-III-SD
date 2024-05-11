This repository is used to download Warcraft III: Reforged(with only SD files) from the Battle.net application

## Prerequisites
1. Battle.net application
2. UAC(or equivalent) to modify `C:\Windows\System32\drivers\etc\hosts`

## Clear cache
1. Close Battle.net application and make sure Agent.exe not running
2. Delete `C:\ProgramData\Battle.net\Agent\data\cache\version-w3*`

## Setup proxy server
1. Download and extract [precompiled files](https://github.com/shuen4/Warcraft-III-SD/releases)
2. Run `Warcraft III SD.exe`
    - in case `(ERROR) failed to write C:\Windows\System32\drivers\etc\hosts`, right click `Warcraft III SD.exe` and run as administrator
3. Wait until `(INFO ) proxy running, ctrl + c to exit`
4. Start Battle.net application and install Warcraft III: Reforged
    - ignore the size in popout window
5. After `Initializing`, you should see 6.2GB size (for 1.36.1.21015)
6. Wait until download complete, switch back application's console, then press CTRL + C to shutdown
    - do not use window close button as it does not preform cleanup
      - if you accidentally close via close button, you can find backup of hosts file in `backup` folder and manual replace it

## Note
1. CDN server is hardcoded as `level3.blizzard.com`
    - almost every file downloaded from this server, changing it to nearest CDN might improve download speed
      - you can change it in source code or `nginx/conf/nginx.conf` when proxy server running and use `nginx.exe -s reload` to reload nginx config
        - list of cdn servers:
          - us.cdn.blizzard.com
          - eu.cdn.blizzard.com
          - kr.cdn.blizzard.com
          - us.cdn.blizzard.com
          - level3.blizzard.com
          - level3.ssl.blizzard.com
          - blzddist1-a.akamaihd.net
          - blzddistkr1-a.akamaihd.net
          - blizzard.gcdn.cloudn.co.kr
2. PATCH server is hardcoded as `us.patch.battle.net`
    - very less file are downloaded from this server, changing it does not have much impact
3. PRODUCT is hardcoded as `w3`
4. CDN PATH is hardcoded as `tpr/war3`
5. CHECKSUM_SIZE is hardcoded as 16 (MD5 size)
    - also keySizeInBytes = 16, checksumSize = 8, blockSizeKb = 4, sizeBytes = 4, offsetBytes = 4
      - these are "more hardcoded" than any previous, changing any of them application will no longer works

## Game note
1. First launch take a little time, for game to download missing files that required for main menu
2. According to my tests, playing with fully downloaded game does not have any issue
3. Battle.net online playing is not tested, use at your own risk
4. The game will download missing file when required, and freeze while downloading
5. CASC tag broken, but seem not having any side effect
6. You might want to disable auto update as well
7. Make sure you are using classic graphic option

## Special thanks
- [nginx](https://nginx.org) for reverse proxy backend
- [wowdev.wiki](https://wowdev.wiki) for [TACT](https://wowdev.wiki/TACT) and [BLTE](https://wowdev.wiki/BLTE)
- [CASCLib](https://github.com/ladislav-zezula/CascLib) for [CascDecompress](https://github.com/ladislav-zezula/CascLib/blob/d9ee87443dbba33ba02858bcdff44d57784a79fb/src/CascDecompress.cpp#L18C7-L18C21) and [DecodeFileFrame](https://github.com/ladislav-zezula/CascLib/blob/d9ee87443dbba33ba02858bcdff44d57784a79fb/src/CascReadFile.cpp#L540C14-L540C29)
- [blizzget](https://github.com/d07RiV/blizzget) for [HttpRequest::get](https://github.com/d07RiV/blizzget/blob/2764ef6a378a1a3e85bf002d946898b417d747d2/src/base/http.cpp#L188)
- [MD5](https://github.com/alm4096/MD5-Hash-Example-VS/tree/7300695ea6def557e7a75dcceb15686c801aae13/MD5) for MD5 algorithms
- [bee.lua](https://github.com/actboy168/bee.lua) for [a2w](https://github.com/actboy168/bee.lua/blob/b4fda4e0865d4369c845015ab2ad45573d6ba245/bee/win/unicode.cpp#L31)
- [ghostpp](https://github.com/uakfdotb/ghostpp) for [UTIL_Replace](https://github.com/uakfdotb/ghostpp/blob/cf397544d08d05f8536272e3ab1b22cf03309d9f/ghost/util.cpp#L615)

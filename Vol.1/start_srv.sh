#!/usr/bin/env bash
(cd ~/srv1 && ./bin/server -p 65500) || (cd ~/srv2 &&./bin/server -p 65502) || (cd ~/srv3 &&./bin/server -p 65504) || (cd ~/srv3 &&./bin/server -p 65506) || (cd ~/srv3 &&./bin/server -p 65508) || (cd ~/srv3 &&./bin/server -p 65510) || echo "Cnnot start server!\n"

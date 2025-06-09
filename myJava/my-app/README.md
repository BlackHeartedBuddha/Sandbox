# Maven Java project for zookeeper testing and apache suite

## Components:
- Zookeeper => global concensus tool among the nodes
- kafkka => distributed event streaming / some similar to rabiitMQ

## Commands 
```bash
# Creating the projecrt
mvn archetype:generate -DgroupId=com.example.app -DartifactId=my-app -Dversion=1.0-SNAPSHOT

# Zookeeper installation
sudo apt install zookeeperd
sudo systemctl start zookeeper
# Or use script
sudo /usr/share/zookeeper/bin/zkServer.sh stop//start

# Building and runnnig target
mvn clean compile
mvn exec:java -Dexec.mainClass="com.mycompany.app.App"

# Container for kafka
docker compose up
```
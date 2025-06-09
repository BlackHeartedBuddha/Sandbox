# Maven Java project for zookeeper testing and apache suite

## Commands to create the project 
```bash
# 
mvn archetype:generate -DgroupId=com.example.app -DartifactId=my-app -Dversion=1.0-SNAPSHOT

# Zookeeper installation
sudo apt install zookeeperd
sudo systemctl start zookeeper
# Or use script
sudo /usr/share/zookeeper/bin/zkServer.sh stop//start


mvn clean compile
mvn exec:java -Dexec.mainClass="com.mycompany.app.App"
```
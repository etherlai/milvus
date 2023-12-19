
echo "start build milvus cmd"
build/builder.sh make
echo "start build milvus image"
docker build -t $1 .
docker push $1

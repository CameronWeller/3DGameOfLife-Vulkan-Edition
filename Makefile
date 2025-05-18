build:
	docker-compose build

run:
	docker-compose run --rm vulkan-engine

clean:
	rm -rf build/

distclean:
	docker-compose down -v
	rm -rf build/ 
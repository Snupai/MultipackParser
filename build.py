import subprocess
import os
import pathlib
import argparse

# Define the necessary paths and settings
PROJECT_DIR = pathlib.Path(__file__).parent.resolve()

parser = argparse.ArgumentParser(description="Build MultipackParser using Docker.")
parser.add_argument('--dockerfile', type=str, default='Dockerfile', help='Dockerfile to use (default: Dockerfile)')
args = parser.parse_args()

DOCKERFILE_PATH = PROJECT_DIR / args.dockerfile
IMAGE_NAME = "multipackparser-builder"
EXECUTABLE_NAME_IN_DOCKER = "MultipackParser"
EXECUTABLE_DIR_IN_DOCKER  = "/app/dist"
EXECUTABLE_PATH_IN_DOCKER = f"{EXECUTABLE_DIR_IN_DOCKER}/{EXECUTABLE_NAME_IN_DOCKER}"
OUTPUT_DIR_ON_HOST = PROJECT_DIR / "output"
OUTPUT_PATH_ON_HOST = OUTPUT_DIR_ON_HOST / EXECUTABLE_NAME_IN_DOCKER

def run_command(command, check=True):
    process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()

    if check and process.returncode != 0:
        raise Exception(f"Command failed with error code {process.returncode}:\n{stderr.decode('utf-8')}")
    else:
        return stdout.decode('utf-8')

def build_docker_image():
    build_command = f"docker buildx build --platform linux/arm64 -t {IMAGE_NAME} --output type=docker -f {DOCKERFILE_PATH} {PROJECT_DIR}"
    print(f"Building Docker image using {DOCKERFILE_PATH}...")
    run_command(build_command)
    print("Docker image built successfully.")

def create_container():
    create_command = f"docker create {IMAGE_NAME}"
    container_id = run_command(create_command).strip()
    print(f"Container created with ID: {container_id}")
    return container_id

def list_files_in_container():
    list_command = f"docker run --rm --platform linux/arm64 {IMAGE_NAME} ls -lR {EXECUTABLE_DIR_IN_DOCKER}"
    files_list = run_command(list_command, check=False)
    print(f"Files in Docker container {EXECUTABLE_DIR_IN_DOCKER}:")
    print(files_list)

def copy_executable(container_id):
    if not os.path.exists(OUTPUT_DIR_ON_HOST):
        os.makedirs(OUTPUT_DIR_ON_HOST)

    # List files in the expected directory to debug the error
    list_files_in_container()

    copy_command = f"docker cp {container_id}:{EXECUTABLE_PATH_IN_DOCKER} {OUTPUT_PATH_ON_HOST}"
    print("Copying executable from container to host...")
    run_command(copy_command)
    print(f"Executable copied to: {OUTPUT_PATH_ON_HOST}")

def remove_container(container_id):
    remove_command = f"docker rm {container_id}"
    print(f"Removing container {container_id}...")
    run_command(remove_command)
    print(f"Container {container_id} removed.")

def main():
    container_id = None  # Initialize container_id to None
    try:
        build_docker_image()
        container_id = create_container()  # Ensure container_id is assigned correctly
        copy_executable(container_id)
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        if container_id:
            remove_container(container_id)

if __name__ == "__main__":
    main()
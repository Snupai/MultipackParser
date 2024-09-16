import os
import subprocess
from dotenv import load_dotenv
import requests
import json
from datetime import datetime
import zipfile
import webbrowser

# Define architectures and image name
architectures = ['linux/arm64']
image_name = 'my_pyside_app'
output_dir = './local_dist'

# Function to run shell commands
def run_command(command):
    try:
        subprocess.check_call(command, shell=True)
    except subprocess.CalledProcessError:
        print(f"Command failed: {command}")
        exit(1)


def clear_folder():
    # Delete the output directory if it exists
    if os.path.exists(output_dir):
        # see if there are .bin files in the output directory
        bin_files = [file for file in os.listdir(output_dir) if file.endswith('.bin')]
        if bin_files:
            print(f"Found .bin files in the output directory: {bin_files}")
            # if there are .bin files create a zip file of both files with the name 'release-binaries_<date>-<time>.zip'
            zip_file_name = f'release-binaries_{datetime.now().strftime("%Y-%m-%d-%H-%M-%S")}.zip'
            with zipfile.ZipFile(zip_file_name, 'w') as zip_file:
                for file in bin_files:
                    zip_file.write(os.path.join(output_dir, file), file)
            print(f"Zip file created: {zip_file_name}")
            # now remove the .bin files from the output directory
            for file in bin_files:
                print(f"Removing {file} from output directory...")
                os.remove(os.path.join(output_dir, file))
            return
        else:
            return


def build_app():
    # Loop over architectures
    for arch in architectures:
        # Build the Docker image for the specific architecture
        build_command = f'docker buildx build --platform {arch} -t {image_name}_{arch} . --load'
        print(f"Building image for {arch}...")
        run_command(build_command)
        
        # Create a temporary container to extract the binaries
        container_id = subprocess.check_output(
            f'docker create {image_name}_{arch}', shell=True
        ).decode().strip()
        
        # Create the architecture-specific output directory if it doesn't exist
        os.makedirs(output_dir, exist_ok=True)
        
        # Copy the built binaries from the container to the local output directory
        copy_command = f'docker cp {container_id}:/app/main.bin {output_dir}'
        run_command(copy_command)

        # rename main.bin to MultipackParser_<arch>.bin 
        os.rename(os.path.join(output_dir, 'main.bin'), os.path.join(output_dir, f'MultipackParser_{arch.replace("/", "_")}.bin'))
        
        # Remove the temporary container
        run_command(f'docker rm {container_id}')
        
        print(f"Exported binary for {arch} to {output_dir}")


def upload_binaries():
    github_token = os.environ.get('GITHUB_TOKEN')
    if not github_token:
        print("GITHUB_TOKEN environment variable not set.")
        return

    # Get the current directory
    current_dir = os.getcwd()

    # Change to the output directory
    os.chdir(output_dir)

    # get latest release version from private github repo
    github_release_url = 'https://api.github.com/repos/Snupai/MultipackParser/releases/latest'
    headers = {'Authorization': f'Bearer {github_token}'}
    response = requests.get(github_release_url, headers=headers)
    latest_release_version = response.json()['tag_name']

    # increment version number
    latest_release_version = latest_release_version.split('.')
    latest_release_version[2] = str(int(latest_release_version[2]) + 1)
    latest_release_version = '.'.join(latest_release_version)

    # create release draft on github with both binaries and the latest release version number
    github_release_url = 'https://api.github.com/repos/Snupai/MultipackParser/releases'
    response = requests.post(github_release_url, headers=headers, data=json.dumps({
        'tag_name': latest_release_version,
        'name': latest_release_version,
        'body': 'This is a release of MultipackParser.',
        'draft': True,
        'prerelease': False
    }))
    release_data = response.json()

    if response.status_code != 201:
        print("Error creating GitHub release:")
        print(release_data)
        return

    # Extract the upload URL template
    upload_url = release_data['upload_url'].replace('{?name,label}', '')

    # upload each binary to github release draft
    for file in os.listdir():
        file_name = os.path.join(file)
        with open(file_name, 'rb') as f:
            file_content = f.read()

        upload_url_with_name = f"{upload_url}?name={file}"
        headers.update({'Content-Type': 'application/octet-stream'})
        response = requests.post(upload_url_with_name, headers=headers, data=file_content)
        print(response.text)

    # open draft release on github in browser for manual review
    print('Opening draft release on GitHub in browser...')
    webbrowser.open(release_data['html_url'])


def main():
    # load environment variables from .env file
    load_dotenv()
    # Ask the user if they want to build the Docker image
    build_docker_image = input("Do you want to build the Docker image? (y/N): ")
    if build_docker_image.lower() == 'y':
        clear_folder()
        build_app()
    else:
        print("Docker image not built.")
        
    
    upload_binaries_to_github = input("Do you want to upload the binaries to GitHub? (y/N): ")
    if upload_binaries_to_github.lower() == 'y':
        upload_binaries()
        clear_folder()
        return
    else:
        print("Binaries not uploaded.")


if __name__ == "__main__":
    main()
else:
    print("I am meant to be run as single file.")
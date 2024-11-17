# Start with a scratch (empty) image
FROM scratch

# Copy the zip file into the image
COPY code.zip /

# Set the entrypoint to do nothing
ENTRYPOINT ["tail", "-f", "/dev/null"]


# Set the entrypoint to do nothing (container will exit immediately if run)
CMD ["echo", "This image contains the zipped code"]
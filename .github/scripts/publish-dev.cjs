const fs = require('node:fs');
const path = require('node:path');

// The workflow serializes publishers; the run marker also rejects delayed old builds.
module.exports = async function publishDev({ github, context, core }) {
  const repository = { owner: context.repo.owner, repo: context.repo.repo };
  const tag = 'dev';
  const assetName = 'gbnh-native-windows-x86_64.zip';
  const runNumber = Number(context.runNumber);
  if (!Number.isSafeInteger(runNumber) || runNumber < 1) {
    throw new Error('A valid Actions run number is required');
  }

  async function optionalResource(request) {
    try {
      return (await request()).data;
    } catch (error) {
      if (error.status === 404) return null;
      throw error;
    }
  }

  let release = await optionalResource(() =>
    github.rest.repos.getReleaseByTag({ ...repository, tag })
  );
  const previousRun = release?.body?.match(/<!-- gbnh-native-dev-run:(\d+) -->/);
  if (previousRun && Number(previousRun[1]) > runNumber) {
    core.info('A newer dev build has already been published; skipping this run.');
    return;
  }

  // Read the complete bundle before modifying any remote state.
  const assetPath = path.join(process.env.GITHUB_WORKSPACE || process.cwd(), assetName);
  const data = fs.readFileSync(assetPath);
  const body = [
    'Rolling development build. This release and its download are updated in place.',
    '',
    `Commit: ${context.sha}`,
    `Ref: ${context.ref}`,
    `Build: ${context.serverUrl}/${repository.owner}/${repository.repo}/actions/runs/${context.runId}`,
    '',
    `<!-- gbnh-native-dev-run:${runNumber} -->`,
  ].join('\n');

  const ref = await optionalResource(() =>
    github.rest.git.getRef({ ...repository, ref: `tags/${tag}` })
  );
  if (ref) {
    await github.rest.git.updateRef({
      ...repository, ref: `tags/${tag}`, sha: context.sha, force: true,
    });
  } else {
    await github.rest.git.createRef({
      ...repository, ref: `refs/tags/${tag}`, sha: context.sha,
    });
  }

  if (!release) {
    release = (await github.rest.repos.createRelease({
      ...repository, tag_name: tag, target_commitish: context.sha,
      name: 'Development build', body, prerelease: true, draft: true,
      make_latest: 'false',
    })).data;
  }

  const assets = await github.paginate(github.rest.repos.listReleaseAssets, {
    ...repository, release_id: release.id, per_page: 100,
  });
  for (const asset of assets) {
    if (asset.name === assetName || asset.name === 'gbnh-native-windows-x86_64-ucrt.zip') {
      await github.rest.repos.deleteReleaseAsset({ ...repository, asset_id: asset.id });
    }
  }
  await github.rest.repos.uploadReleaseAsset({
    ...repository, release_id: release.id, name: assetName, data,
    headers: { 'content-type': 'application/zip', 'content-length': data.length },
  });
  await github.rest.repos.updateRelease({
    ...repository, release_id: release.id, name: 'Development build',
    body, prerelease: true, draft: false, make_latest: 'false',
  });
  core.info(`Updated the dev prerelease to ${context.sha}.`);
};
